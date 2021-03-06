#include "CX_SoundBufferPlayer.h"

namespace CX {

CX_SoundBufferPlayer::CX_SoundBufferPlayer(void) :
	_soundStream(nullptr),
	_soundStreamSelfAllocated(false),
	_listeningForEvents(false),
	_buffer(nullptr),
	_withinOutputEvent(false),
	_playing(false),
	_playbackStartQueued(false),
	_playbackStartSampleFrame(std::numeric_limits<uint64_t>::max()),
	_currentSampleFrame(0),
	_soundPlaybackSampleFrame(0)
{}

CX_SoundBufferPlayer::~CX_SoundBufferPlayer(void) {
	this->stop();
	if (_soundStream != nullptr) {
		_listenForEvents(false);
		if (_soundStreamSelfAllocated) {
			_soundStream->closeStream();
			delete _soundStream;
			_soundStreamSelfAllocated = false;
		}
	}
}

/*! Configures the CX_SoundBufferPlayer with the given configuration. A CX_SoundStream will be set
up within the CX_SoundBufferPlayer and the sound stream will be started.
\param config The configuration to use for the CX_SoundBufferPlayer, which is really all about configuring
the CX_SoundStream used internally by the CX_SoundBufferPlayer.
*/
bool CX_SoundBufferPlayer::setup(Configuration config) {
	_cleanUpOldSoundStream();

	_soundStream = new CX_SoundStream;
	_soundStreamSelfAllocated = true;
	_listenForEvents(true);

	bool setupSuccessfully = _soundStream->setup((CX_SoundStream::Configuration&)config);

	if (_buffer != nullptr) {
		setSoundBuffer(_buffer);
	}

	bool startedSuccessfully = _soundStream->start();

	return startedSuccessfully && setupSuccessfully;
}

/*! Set up the sound buffer recorder from an existing CX_SoundStream. The CX_SoundStream is not started automatically.
The CX_SoundStream must exist for the lifetime of the CX_SoundBufferPlayer.
\param ss A pointer to a fully configured CX_SoundStream.
\return `true` in all cases. */
bool CX_SoundBufferPlayer::setup(CX_SoundStream *ss) {
	_cleanUpOldSoundStream();

	_soundStream = ss;
	_soundStreamSelfAllocated = false;
	_listenForEvents(true);

	if (_buffer != nullptr) {
		setSoundBuffer(_buffer);
	}

	return true;
}

/*! Attempts to start playing the current CX_SoundBuffer associated with the player.

\return `true` if the sound buffer associated with the player isReadyToPlay(), `false` otherwise. */
bool CX_SoundBufferPlayer::play(void) {
	if (_soundStream == nullptr) {
		CX::Instances::Log.error("CX_SoundBufferPlayer") << "play(): Could not start sound playback, the sound stream was nonexistent. Have you forgotten to call setup()?";
		return false;
	}

	if (!_soundStream->isStreamRunning()) {
		CX::Instances::Log.error("CX_SoundBufferPlayer") << "Could not start sound playback. The sound stream was not running.";
		return false;
	}

	if ((_buffer != nullptr) && _buffer->isReadyToPlay()) {
		_playing = true;
		_soundPlaybackSampleFrame = 0;
		return true;
	}
	CX::Instances::Log.error("CX_SoundBufferPlayer") << "Could not start sound playback. There was a problem with the sound "
		"buffer associated with the player. Have you remembered to call setSoundBuffer()?";
	return false;
}

/*! Stop the currently playing sound buffer, or, if a playback start was cued, cancel the cued playback.
\return Always returns `true`. */
bool CX_SoundBufferPlayer::stop(void) {
	_playing = false;
	//_startTime = std::numeric_limits<CX_Micros>::max();
	return true;
}

/*! Queue the start time of the sound in experiment time with an offset to account for latency. 

The start time is adjusted by an estimate of the latency of the sound stream. This is calculated
as (N_b - 1) * S_b / SR, where N_b is the number of buffers, S_b is the size of the buffers (in sample 
frames), and SR is the sample rate, in sample frames per second.

In order for this function to have any meaningful effect, the request start time, plus any latency 
adjustments, must be in the future. If `experimentTime` plus `latencyOffset` minus the estimated 
stream latency is not in the future, the sound will start playing immediately and a warning will be logged.

\param experimentTime The desired experiment time at which the sound should start playing.
\param latencyOffset An offset that accounts for latency. If, for example, you called this function with
an offset of 0 and discovered that the sound played 200 ms later than you were expecting it to,
you would set offset to -200 in order to queue the start time 200 ms earlier than the
desired experiment time.
\return `false` if the start time plus the latency offset is in the past. `true` otherwise.
\note See CX_SoundBufferPlayer::seek() for a way to choose the current time point within the sound.
*/
bool CX_SoundBufferPlayer::startPlayingAt(CX_Millis experimentTime, CX_Millis latencyOffset) {
	if (_soundStream == nullptr) {
		CX::Instances::Log.error("CX_SoundBufferPlayer") << "startPlayingAt(): Could not queue sound playback start, the sound stream was nonexistent. Have you forgotten to call setup()?";
		return false;
	}

	CX_Millis partialStreamLatency = _soundStream->estimateTotalLatency() - _soundStream->estimateLatencyPerBuffer();

	CX_Millis adjustedStartTime = experimentTime + latencyOffset - partialStreamLatency;

	if (adjustedStartTime <= CX::Instances::Clock.now()) {
		CX::Instances::Log.warning("CX_SoundBufferPlayer") << "startPlayingAt: Desired start time has already passed. Starting immediately.";
		play();
		return false;
	}

	CX_Millis lastSwapTime = _soundStream->getLastSwapTime(); //This is the time at which the last swap started (i.e. as soon as the fill buffer callback was called).
	uint64_t samplesFramesSinceLastSwap = (adjustedStartTime - lastSwapTime).seconds() * this->getConfiguration().sampleRate;

	uint64_t lastSwapStartSFNumber = _soundStream->getSampleFrameNumber() - this->getConfiguration().bufferSize; //Go back to the previous buffer start SF

	_playbackStartSampleFrame = lastSwapStartSFNumber + samplesFramesSinceLastSwap;
	_playbackStartQueued = true;
	return true;
}

/*! Set the current time in the active sound. When playback starts, it will begin from that
time in the sound. If the sound buffer is currently playing, this will jump to that point
in the sound.
\param time The time in the sound to seek to.
\note If used while the sound is playing, a warning will be logged but the function will still
have its normal effect.
*/
void CX_SoundBufferPlayer::seek(CX_Millis time) {
	if (_playing) {
		CX::Instances::Log.warning("CX_SoundBufferPlayer") << "seek() used while sound was playing.";
		while (_withinOutputEvent)
			;
	}
	_soundPlaybackSampleFrame = time.seconds() * this->getConfiguration().sampleRate;
}

/*! Returns the configuration used for this CX_SoundBufferPlayer. */
CX_SoundBufferPlayer::Configuration CX_SoundBufferPlayer::getConfiguration(void) {
	if (_soundStream == nullptr) {
		CX::Instances::Log.error("CX_SoundBufferPlayer") << "getConfiguration(): Could not get configuration, the sound stream was nonexistent. Have you forgotten to call setup()?";
		return CX_SoundBufferPlayer::Configuration();
	}

	return (CX_SoundBufferPlayer::Configuration)_soundStream->getConfiguration();
}

/*!
This function is potentially blocking because the sample rate and number of channels of sound are changed to those of the currently
open stream if they do not already match (see \ref blockingCode).
\param sound A pointer to a CX_SoundBuffer that will be set as the current sound for the CX_SoundBufferPlayer. There are a variety of
reasons why the sound could fail to be set as the current sound for the player. If sound was not loaded successfully, this function
call fails and an error is logged. If it is not possible to convert the number of channels of sound to the number of channels that
the CX_SoundBufferPlayer is configured to use, this function call fails and an error is logged.

This function call is not blocking if the same rate and channel count of the CX_SoundBuffer are the same as those in use by
the CX_SoundBufferPlayer. See \ref blockingCode for more information.

\return True if sound was successfully set to be the current sound, false otherwise.
*/
bool CX_SoundBufferPlayer::setSoundBuffer(CX_SoundBuffer *sound) {
	if (_soundStream == nullptr) {
		CX::Instances::Log.error("CX_SoundBufferPlayer") << "setSoundBuffer(): You cannot set the sound buffer until the CX_SoundBufferPlayer has been set up. Call setup() first.";
		return false;
	}

	if (sound == nullptr) { //This check is redundant, in that nullptr is checked for in play(), but this catches it earlier.
		return false;
	}

	//I'm not entirely sure what this check is for. What exactly am I wanting to know about the status of the sound buffer?
	if (!sound->isLoadedSuccessfully()) {
		CX::Instances::Log.error("CX_SoundBufferPlayer") << "Sound is not loaded successfully. It will not be set as the active sound.";
		return false;
	}

	_playing = false; //Stop playback of the current sound.

	const CX_SoundStream::Configuration &streamConfig = this->getConfiguration();

	if (streamConfig.outputChannels != sound->getChannelCount()) {
		if (!sound->setChannelCount(streamConfig.outputChannels)) {
			CX::Instances::Log.error("CX_SoundBufferPlayer") << "It was not possible to change the number of channels of the sound to the number used by the sound player.";
			return false;
		}
		CX::Instances::Log.warning("CX_SoundBufferPlayer") << "Channel count changed: Sound fidelity may have been lost.";
	}

	if (streamConfig.sampleRate != sound->getSampleRate()) {
		CX::Instances::Log.warning("CX_SoundBufferPlayer") << "Sound resampled: Sound fidelity may have been lost.";
		sound->resample((float)streamConfig.sampleRate);
	}

	_buffer = sound;

	return true;
}

/*! This function provides access to the CX_SoundBuffer that is in use by the CX_SoundBufferPlayer.
If this function is called during playback of the sound buffer, a warning will be logged, but the 
buffer pointer will still be returned. 
\return A pointer to the CX_SoundBuffer that is currently assigned to the CX_SoundBufferPlayer. */
CX_SoundBuffer* CX_SoundBufferPlayer::getSoundBuffer(void) {
	if (_playing) {
		CX::Instances::Log.warning("CX_SoundBufferPlayer") << "getSoundBuffer: Sound buffer pointer accessed while playback was in progress.";
	}
	return _buffer;
}

bool CX_SoundBufferPlayer::_outputEventHandler(CX_SoundStream::OutputEventArgs &outputData) {
	
	if ((!_playing && !_playbackStartQueued) || (_buffer == nullptr)) {
		return false;
	}

	_withinOutputEvent = true;

	uint64_t sampleFramesToOutput = outputData.bufferSize;
	uint64_t outputBufferOffset = 0;
	std::vector<float> &soundData = _buffer->getRawDataReference();
	const CX_SoundStream::Configuration &config = this->getConfiguration();

	if (_playbackStartQueued) {
		if (_playbackStartSampleFrame < (_currentSampleFrame + outputData.bufferSize)) {
			_playing = true;
			_playbackStartQueued = false;

			outputBufferOffset = _playbackStartSampleFrame - _currentSampleFrame;
			sampleFramesToOutput = outputData.bufferSize - outputBufferOffset;
			_soundPlaybackSampleFrame = 0;
		} else {
			sampleFramesToOutput = 0;
		}
	}

	if (soundData.size() < ((_soundPlaybackSampleFrame + outputData.bufferSize - outputBufferOffset) * config.outputChannels)) {
		//If there is not enough data to completely fill the sound buffer, so only use some of it.
		sampleFramesToOutput = (soundData.size() / config.outputChannels) - _soundPlaybackSampleFrame - outputBufferOffset;
		_playing = false;
	}

	//Copy over the data, adding to the existing data. Addition allows multiple CX_SoundBufferPlayers to play into
	//the same sound stream at the same time.
	if (sampleFramesToOutput > 0) {
		unsigned int rawSamplesToOutput = sampleFramesToOutput * config.outputChannels; //not times sizeof(float)!
		float *rawData = soundData.data() + (_soundPlaybackSampleFrame * config.outputChannels);
		float *dataTarget = outputData.outputBuffer + outputBufferOffset;

		for (unsigned int i = 0; i < rawSamplesToOutput; i++) {
			dataTarget[i] += rawData[i];
		}
	}

	_currentSampleFrame += outputData.bufferSize;

	_soundPlaybackSampleFrame += sampleFramesToOutput;

	_withinOutputEvent = false;
	return true;
}

void CX_SoundBufferPlayer::_listenForEvents(bool listen) {
	if ((listen == _listeningForEvents) || (_soundStream == nullptr)) {
		return;
	}

	if (listen) {
		ofAddListener(_soundStream->outputEvent, this, &CX_SoundBufferPlayer::_outputEventHandler);
	} else {
		ofRemoveListener(_soundStream->outputEvent, this, &CX_SoundBufferPlayer::_outputEventHandler);
	}

	_listeningForEvents = listen;
}

void CX_SoundBufferPlayer::_cleanUpOldSoundStream(void) {
	//If another sound stream was connected, stop listening to it.
	if (_soundStream != nullptr) {
		_listenForEvents(false);

		if (_soundStreamSelfAllocated) {
			delete _soundStream;
			_soundStreamSelfAllocated = false;
		}
	}
}

} //namespace CX;
