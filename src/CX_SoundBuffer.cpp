#include "CX_SoundBuffer.h"

namespace CX {

CX_SoundBuffer::CX_SoundBuffer(void) :
	_successfullyLoaded(false),
	_soundChannels(0)
{}

/*!
Loads a sound file with the given file name into the CX_SoundBuffer. Any pre-existing data in the CX_SoundBuffer is deleted.
Some sound file types are supported. Others are not. In the limited testing, mp3 and wav files seem to work well.
If the file cannot be loaded, descriptive error messages will be logged.

\param fileName Name of the sound file to load.

\return True if the sound given in the fileName was loaded succesffuly, false otherwise.
*/
bool CX_SoundBuffer::loadFile (string fileName) {
	_successfullyLoaded = true;

	ofFmodSoundPlayer fmPlayer;
	bool loadSuccessful = fmPlayer.loadSound(fileName, false);
	if (!loadSuccessful) {
		CX::Instances::Log.error("CX_SoundBuffer") << "Error loading " << fileName;
		fmPlayer.unloadSound(); //Just in case.
		_successfullyLoaded = false;
		return _successfullyLoaded;
	}

	FMOD_SOUND *fmSound = fmPlayer.sound;

	FMOD_SOUND_TYPE soundType; //I'm not sure what to do with this, either. Check that it isn't a bizzare unreadable type, maybe?
	FMOD_SOUND_FORMAT soundFormat;
	int channels;
	int bits; //I'm not really sure what to do with this that soundFormat doesn't give me.

	FMOD_RESULT formatResult = FMOD_Sound_GetFormat( fmSound, &soundType, &soundFormat, &channels, &bits );
	if (formatResult != FMOD_OK) {
		CX::Instances::Log.error("CX_SoundBuffer") << "Error getting sound format of " << fileName;
		fmPlayer.unloadSound();
		_successfullyLoaded = false;
		return _successfullyLoaded;
	}

	_soundChannels = channels;
	_soundFormat = soundFormat;

	//Find out the sample rate of the sound. To be used if it needs to be resampled.
	//At least, I think this gets the sample rate of the sound. The documentation isn't clear.
	FMOD_Sound_GetDefaults( fmSound, &_soundSampleRate, NULL, NULL, NULL );

	void *ptr1;
	void *ptr2;
	unsigned int length1;
	unsigned int length2;

	switch (soundFormat) {
	case FMOD_SOUND_FORMAT_PCM8:
		CX::Instances::Log.error("CX_SoundOutput") << "File " << fileName << " is in an unsupported format (8-bit PCM). FMOD_SOUND_FORMAT_PCM8 not yet supported.";
		_successfullyLoaded = false;
		break;
	case FMOD_SOUND_FORMAT_PCM16:
		{
			unsigned int samplesToRead =  fmPlayer.length; // * channelCount ????

			FMOD_RESULT lockResult = FMOD_Sound_Lock( fmSound, 0, samplesToRead, &ptr1, &ptr2, &length1, &length2 );

			if (lockResult == FMOD_OK) {
				unsigned int totalSamples = length1 * channels; //The documentation says that length1 is in bytes, but it seems to be wrong. 
				//It is in samples for everything I've found (which have been entirely 16-bit PCM).
				_soundData.resize( totalSamples );
				
				for (unsigned int i = 0; i < totalSamples; i++) {
					_soundData[i] = ((float)(((int16_t*)ptr1)[i]))/32768;
				}
			} else {
				CX::Instances::Log.error("CX_SoundOutput") << "Error locking sound data";
				_successfullyLoaded = false;
			}

			FMOD_RESULT unlockResult = FMOD_Sound_Unlock( fmSound, ptr1, ptr2, length1, length2 );
		}
		break;
	case FMOD_SOUND_FORMAT_PCM24:
		//This is annoying because the sign must be extended. Maybe should also be processed as 64-bit float.
		CX::Instances::Log.error("CX_SoundOutput") << "File " << fileName << " is in an unsupported format (24-bit PCM). FMOD_SOUND_FORMAT_PCM24 not yet supported.";
		_successfullyLoaded = false;
		break;
	case FMOD_SOUND_FORMAT_PCM32:
		//This is annoying because it must be processed as 64-bit float before being converted back to 32-bit float.
		CX::Instances::Log.error("CX_SoundOutput") << "File " << fileName << " is in an unsupported format (32-bit PCM). FMOD_SOUND_FORMAT_PCM32 not yet supported.";
		_successfullyLoaded = false;
		break;
	case FMOD_SOUND_FORMAT_PCMFLOAT:
		//This is awesome because I don't have to do anything at all with it!
		{
			unsigned int samplesToRead = fmPlayer.length; // * channelCount ????

			FMOD_RESULT lockResult = FMOD_Sound_Lock( fmSound, 0, samplesToRead, &ptr1, &ptr2, &length1, &length2 );

			if (lockResult == FMOD_OK) {
				unsigned int totalSamples = length1 * channels;
				_soundData.resize( totalSamples );
				memcpy(_soundData.data(), ptr1, totalSamples * sizeof(float));
			} else {
				CX::Instances::Log.error("CX_SoundOutput") << "Error locking sound data";
				_successfullyLoaded = false;
			}

			FMOD_RESULT unlockResult = FMOD_Sound_Unlock( fmSound, ptr1, ptr2, length1, length2 );
		}
		break;
	case FMOD_SOUND_FORMAT_NONE:
		CX::Instances::Log.error("CX_SoundOutput") << "File " << fileName << " of unknown format.";
		_successfullyLoaded = false;
		break;
	case FMOD_SOUND_FORMAT_GCADPCM:
	case FMOD_SOUND_FORMAT_IMAADPCM:
	case FMOD_SOUND_FORMAT_VAG:
	case FMOD_SOUND_FORMAT_XMA:
	case FMOD_SOUND_FORMAT_MPEG:
	case FMOD_SOUND_FORMAT_MAX:
	case FMOD_SOUND_FORMAT_FORCEINT:
		CX::Instances::Log.error("CX_SoundOutput") << "File " << fileName << " is of unsupported file format (compressed/video game console). There are no plans to ever support these formats.";
		_successfullyLoaded = false;
		break;
	};

	//Clean up by unloading this sound.
	fmPlayer.unloadSound();

	if (_successfullyLoaded) {
		name = fileName;
	}

	return _successfullyLoaded;
}

/*!
Uses loadFile(string) and addSound(CX_SoundBuffer, uint64_t) to add the given file to the current CX_SoundBuffer at the given time offset (in microseconds).
See those functions for more information.

\param fileName Name of the sound file to load.
\param timeOffset Time at which to add the new sound.

\return Returns true if the new sound was added sucessfully, false otherwise.
*/
bool CX_SoundBuffer::addSound(std::string fileName, CX_Millis timeOffset) {
	if (_soundData.size() == 0 || !this->_successfullyLoaded) {
		bool loadSuccess = this->loadFile(fileName);
		if (loadSuccess) {
			this->addSilence(timeOffset, true);
		}
		return loadSuccess;
	} else {
		CX_SoundBuffer temp;
		if (!temp.loadFile(fileName)) {
			return false;
		}
		addSound(temp, timeOffset);
		this->_successfullyLoaded = true; //What is this doing here?
		return true;
	}
}

/*!
Adds the sound data in `nsb` at the time offset. If the sample rates of the sounds differ, nsb will be resampled to the sample rate of this CX_SoundBuffer.
If the number of channels of nsb does not equal the number of channels of this, an attempt will be made to set the number of channels of
nsb equal to the number of channels of this CX_SoundBuffer.
The data from `nsb` and this CX_SoundBuffer are merged by adding the amplitudes of the sounds. The result of the addition is clamped between -1 and 1.

\param nsb A CX_SoundBuffer. Must be successfully loaded.
\param timeOffset Time at which to add the new sound data in microseconds. Dependent on sample rate.

\return True if nsb was successfully added to this CX_SoundBuffer, false otherwise.
*/
bool CX_SoundBuffer::addSound(CX_SoundBuffer nsb, CX_Millis timeOffset) {
	if (!nsb._successfullyLoaded) {
		CX::Instances::Log.error("CX_SoundBuffer") << "addSound: Added sound buffer not successfully loaded. It will not be added.";
		return false;
	}

	//This condition really should have a warning message associated with it.
	if (!this->_successfullyLoaded) {
		*this = nsb;
		this->addSilence(timeOffset, true);
		return true;
	}

	if (nsb.getSampleRate() != this->getSampleRate()) {
		nsb.resample( this->getSampleRate() );
	}

	if (nsb.getChannelCount() != this->getChannelCount()) {
		if (!nsb.setChannelCount( this->getChannelCount() )) {
			CX::Instances::Log.error("CX_SoundBuffer") << "addSound: Failed to match the number of channels of added sound to existing sound. The new sound will not be added.";
			return false;
		}
	}

	//Time is in microseconds, so do samples/second * seconds * channels to get the (absolute) sample at which the new sound starts.
	unsigned int insertionSample = _soundChannels * (unsigned int)(this->getSampleRate() * timeOffset.seconds());

	//Get the new data that will be merged.
	vector<float> &newData = nsb.getRawDataReference();
	
	//If this sound isn't long enough to hold the new data, resize it to fit.
	if (insertionSample + newData.size() > this->_soundData.size()) {
		_soundData.resize( insertionSample + newData.size(), 0 ); //When resizing, set any new elements to silence (i.e. 0).
	}

	//Copy over the new data, clamping as needed.
	for (unsigned int i = 0; i < newData.size(); i++) {
		float newVal = _soundData[insertionSample + i] + newData[i];
		newVal = std::max(newVal, -1.0f);
		newVal = std::min(newVal, 1.0f);
		_soundData[insertionSample + i] = newVal;
	}

	return true;
}

/*! Set the contents of the sound buffer from a vector of float data. 
\param data A vector of sound samples. These values should go from -1 to 1. This requirement is not checked for. 
If there is more than once channel of data, the data must be interleaved. This means that if, for example, 
there are two channels, the ordering of the samples is 12121212... where 1 represents a sample for channel 
1 and 2 represents a sample for channel 2. This requirement is not checked for. The number of samples in this
vector must be evenly divisible by the number of channels set with the `channels` argument, which is checked for!
\param channels The number of channels worth of data that is stored in `data`.
\param sampleRate The sample rate of the samples. If `data` contains, for example, a sine wave, that wave was sampled
at some rate (e.g. 48000 samples per second of waveform). `sampleRate` should be that rate.
return True in all cases. No checking is done on any of the arguments.
*/
bool CX_SoundBuffer::setFromVector(const std::vector<float>& data, int channels, float sampleRate) {
	if ((data.size() % channels) != 0) {
		CX::Instances::Log.error("CX_SoundBuffer") << "setFromVector: The size of the sample data was not evenly divisible by the number of channels.";
		return false;
	}


	_soundData = data;
	_soundChannels = channels;
	_soundSampleRate = sampleRate;
	_successfullyLoaded = true; //Do no checking of the values.
	return true;
}

/*! Set the contents of a single channel from a vector of float data.
\param channel The channel to set the data for. If greater than any existing channel, new channels will be created
so that the number of stored channels is equal to `channel + 1`. If you don't want a bunch of new empty channels, make sure
you don't use a large channel number.
\param data A vector of sound samples. These values must be in the interval [-1, 1], which is not checked for. 
See CX::Util::clamp() for one method of making sure your data are in the correct range.
If the other channels in the CX_SoundBuffer are longer than `data`, `data` will be extended with zeroes.
If the other channels in the CX_SoundBuffer are shorter than `data`, those channels will be extended with zeroes.

*/
void CX_SoundBuffer::setChannelData(unsigned int channel, const std::vector<float>& data) {

	if (channel >= _soundChannels) {
		this->setChannelCount(channel + 1, false); //average = false?
	}

	for (unsigned int sampleFrame = 0; sampleFrame < data.size(); sampleFrame++) {
		unsigned int index = (sampleFrame * _soundChannels) + channel;
		_soundData[index] = data[sampleFrame];
	}

}

/*! Checks to see if the CX_SoundBuffer is ready to play. It basically just checks if there is sound data
available and that the number of channels is set to a sane value. */
bool CX_SoundBuffer::isReadyToPlay (void) {
	return ((_soundChannels > 0) && (_soundData.size() != 0)); //_successfullyLoaded? _soundSampleRate? Remove _soundChannels?
}

/*! Set the length of the sound to the specified length in microseconds. If the new length is longer than the old length,
the new data is zeroed (i.e. set to silence). */
void CX_SoundBuffer::setLength(CX_Millis length) {
	unsigned int endOfDurationSample = _soundChannels * (unsigned int)(getSampleRate() * length.seconds());

	_soundData.resize( endOfDurationSample, 0 );
}

/*! Gets the length, in time, of the data stored in the sound buffer. This depends on the sample rate of the sound.
\return The length. */
CX_Millis CX_SoundBuffer::getLength(void) {
	return CX_Seconds((double)_soundData.size() / (getChannelCount() * (double)getSampleRate()));
	//return (long long)(((uint64_t)_soundData.size() * 1000000)/(getChannelCount() * (uint64_t)getSampleRate()));
}


/*! Finds the maximum amplitude in the sound buffer.
\return The maximum amplitude.
\note Amplitudes are between -1 and 1, inclusive. */
float CX_SoundBuffer::getPositivePeak (void) {
	return Util::max(_soundData);
}

/*! Finds the minimum amplitude in the sound buffer.
\return The minimum amplitude.
\note Amplitudes are between -1 and 1, inclusive. */
float CX_SoundBuffer::getNegativePeak (void) {
	return Util::min(_soundData);
}

/*! Normalizes the contents of the sound buffer.
\param amount Must be in the interval [0,1]. The peak with the greatest absolute amplitude will be
set to `amount` and all other samples will be scaled proportionally so as to retain their relationship with
the greatest absolute peak.
*/
void CX_SoundBuffer::normalize(float amount) {
	float peak = std::max(abs(getPositivePeak()), abs(getNegativePeak()));
	float multiplier = amount / peak;

	for (unsigned int i = 0; i < _soundData.size(); i++) {
		_soundData[i] *= multiplier;
	}
}

/*!
Removes leading "silence" from the sound, where silence is defined by the given tolerance. It is unlikely that
the beginning of a sound, even if perceived as silent relative to the rest of the sound, has an amplitude of 0.
Therefore, a tolerance of 0 is unlikely to prove useful. Using getPositivePeak() and/or getNegativePeak() can
help to give a reference amplitude of which some small fraction is perceived as "silent".

\param tolerance All sound data up to and including the first instance of a sample with an amplitude
with an absolute value greater than or equal to tolerance is removed from the sound.
*/
void CX_SoundBuffer::stripLeadingSilence (float tolerance) {
	for (unsigned int sampleFrame = 0; sampleFrame < this->getSampleFrameCount(); sampleFrame++) {
		for (int channel = 0; channel < _soundChannels; channel++) {
			unsigned int index = (sampleFrame * _soundChannels) + channel;

			if (abs(_soundData.at(index)) >= tolerance) {
				_soundData.erase(_soundData.begin(), _soundData.begin() + (sampleFrame * _soundChannels));
				return;
			}
		}
	}
}

/*! Adds the specified amount of silence to the CX_SoundBuffer at either the beginning or end.

\param duration Duration of added silence in microseconds. Dependent on the sample rate of the sound. If the sample rate changes,
so does the duration of silence.
\param atBeginning If true, silence is added at the beginning of the CX_SoundBuffer. If false, the silence is added at the end.
*/
void CX_SoundBuffer::addSilence(CX_Millis duration, bool atBeginning) {
	//Time is in microseconds, so do samples/second * seconds * channels to get the absolute sample count for the new silence.
	unsigned int absoluteSampleCount = _soundChannels * (unsigned int)(getSampleRate() * duration.seconds());

	if (atBeginning) {
		_soundData.insert( _soundData.begin(), absoluteSampleCount, 0 );
	} else {
		_soundData.insert( _soundData.end(), absoluteSampleCount, 0 );
	}
}

/*! Deletes the specified amount of sound from the CX_SoundBuffer from either the beginning or end.

\param duration Duration of removed sound in microseconds. If this is greater than the duration of the sound, the whole sound is deleted.
\param fromBeginning If true, sound is deleted from the beginning of the CX_SoundBuffer's buffer.
If false, the sound is deleted from the end, toward the beginning.
*/
void CX_SoundBuffer::deleteAmount(CX_Millis duration, bool fromBeginning) {
	//Time is in microseconds, so do samples/second * seconds * channels to get the absolute sample count to delete.
	unsigned int absoluteSampleCount = _soundChannels * (unsigned int)(getSampleRate() * duration.seconds());

	if (absoluteSampleCount >= _soundData.size()) {
		_soundData.clear();
	} else {
		if (fromBeginning) {
			_soundData.erase( _soundData.begin(), _soundData.begin() + absoluteSampleCount );
		} else {
			_soundData.erase( _soundData.end() - absoluteSampleCount, _soundData.end() );
		}
	}
}

/*! Delete the specified channel from the data.
\param channel A 0-indexed index of the channel to delete.
\return `true` if there were no errors.
*/
bool CX_SoundBuffer::deleteChannel(unsigned int channel) {
	if (channel >= this->getChannelCount()) {
		CX::Instances::Log.error("CX_SoundBuffer") << "deleteChannel(): Specified channel does not exist.";
		return false;
	}

	std::vector<float> dataCopy;

	for (unsigned int sampleFrame = 0; sampleFrame < this->getSampleFrameCount(); sampleFrame++) {
		for (int ch = 0; ch < this->getChannelCount(); ch++) {
			if (ch != channel) {
				unsigned int index = (sampleFrame * this->getChannelCount()) + ch;
				dataCopy.push_back( this->_soundData[index] );
			}
		}
	}

	_soundChannels -= 1;
	_soundData = dataCopy;
	return true;
}

/*!
Sets the number of channels of the sound. Depending on the old number of channels (O) and the new number of channels (N),
the conversion is performed in different ways. The cases in this list are evaluated in order an only 1 is executed, so a 
later case cannot be reached if an earlier case has already evaluated to true. When a case says anything about the average
of existing data, it of course means the average on a sample-by-sample basis, not the average of all the samples.

+ If `O == N`, nothing happens.
+ If `O == 0`, the number of channels is just set to N. However, `O == 0`, that usually means that there is no sound data available, so changing the number of channels is kind of meaningless.
+ If `N == 0`, the CX_SoundBuffer is cleared: all data is deleted. If you have no channels, you cannot have data in those channels.
+ If `O == 1`, each of the `N` new channels is set equal to the value of the single old channel. 
+ If `N == 1`, and `average == true` the new channel is set equal to the average of the `O` old channels. If `average == false`, the `O - N` old channels are simply removed.
+ If `N > O`, the first `O` channels are preserved unchanged. If `average == true`, the `N - O` new channels are set to the average of the `O` old channels.
If `average == false`, the `N - O` new channels are set to 0.
+ If `N < O`, and `average == false`, the data from the `O - N` to-be-removed channels is discarded. 
If `average == true` the data from the `O - N` to-be-removed channels are averaged and added on to the `N` remaining channels. 
The averaging is done in an unusual way, so that the average intensitity of the kept channels is equal to the average intensity of the removed channels. 
An example to show why this is done:
Assume that you have 3 channels -- a, b, and c -- and are switching to 2 channels, removing c. The average of c is just c, so when c is added to a and b, you now have c in 2 channels, 
whereas it was just in 1 channel originally: (a + c) + (b + c) = a + b + 2c. Thus, the final intensity of c is too high.
What we want to do is scale c down by the number of channels it is being added to so that the total amount of c is equal
both before and after changing the number of channels, so you divide c by the number of channels it is being added to (2).
Now, (a + c/2) + (b + c/2) = a + b + c.
However, there is another problem, which is that abs(a + c/2) can be greater than 1 even if the absolute value of both is no greater than 1. 
Now we need to scale each sample so that it is constrained to the proper range. 
We do that by multiplying by the number of kept channels (2) by the original number of channels (3).
Now we have 2/3 * (a + c/2) = 2a/3 + c/3, which is bounded between -1 and 1, as long as a and c are both bounded.
Also, 2/3 * [(a + c/2) + (b + c/2)] = 2a/3 + 2b/3 + 2c/3, so the ratios of the components of the original sound are equal.



\param newChannelCount The number of channels the CX_SoundBuffer will have after the conversion.
\param average If `true` and case `N < O` is reached, then the `O - N` old channels that are being removed will be averaged and this
average will be added back into the `N` remaining channels. If `false` (the default), the channels that are being removed will actually be removed.

\return `true` if the conversion was successful, `false` if the attempted conversion is unsupported.
*/
bool CX_SoundBuffer::setChannelCount (unsigned int newChannelCount, bool average) {

	unsigned int O = _soundChannels;
	unsigned int N = newChannelCount;

	if (O == N) {
		return true;
	}

	if (O == 0) {
		//If there are no old channels, just set the channel count to the new value. There can be no data to copy to new channels, 
		//because there were no channels to contain the data.
		_soundChannels = newChannelCount;
		return true;
	}

	if (N == 0) {
		//If there are no new channels, just clear the data
		this->clear();
		return true;
	}

	if (O == 1) {
		//Mono to anything is easy: Just copy the data to new channels.

		unsigned int originalSize = _soundData.size();
		_soundData.resize( _soundData.size() * newChannelCount );
		
		for (unsigned int samp = 0; samp < originalSize; samp++) {
			for (unsigned int ch = 0; ch < newChannelCount; ch++) {
				unsigned int destIndex = _soundData.size() - 1 - (samp * newChannelCount) - ch;
				unsigned int sourceIndex = originalSize - samp - 1;
				_soundData[destIndex]  = _soundData[sourceIndex];
			}
		}

		_soundChannels = newChannelCount;
		return true;
	}
	
	if (N == 1) {
		std::vector<float> newSoundData(this->getSampleFrameCount());

		//Anything to mono is easy: just average all sample frames.
		if (average) {
			for (unsigned int outputSamp = 0; outputSamp < newSoundData.size(); outputSamp++) {
				float avg = 0;
				for (unsigned int ch = 0; ch < _soundChannels; ch++) {
					avg += _soundData[(outputSamp * _soundChannels) + ch];
				}
				avg /= (float)_soundChannels;

				newSoundData[outputSamp] = avg;
			}
		} else {
			//Remove all but the first channel
			for (unsigned int outputSamp = 0; outputSamp < newSoundData.size(); outputSamp++) {
				newSoundData[outputSamp] = _soundData[(outputSamp * _soundChannels) + 0];
			}

		}

		_soundChannels = newChannelCount;
		_soundData = newSoundData;
		return true;
	}

	if (N > O) {

		std::vector<float> newSoundData( this->getSampleFrameCount() * newChannelCount );

		if (average) {
			//New channels set to average of existing channels
			for (unsigned int sample = 0; sample < getSampleFrameCount(); sample++) {
				float average = 0;
				for (unsigned int oldChannel = 0; oldChannel < _soundChannels; oldChannel++) {
					float samp = _soundData[ (sample * _soundChannels) + oldChannel ];
					average += samp;
					newSoundData[ (sample * newChannelCount) + oldChannel ] = samp;
				}
				average /= _soundChannels;

				for (unsigned int newChannel = _soundChannels; newChannel < newChannelCount; newChannel++) {
					newSoundData[ (sample * newChannelCount) + newChannel ] = average;
				}
			}
		} else {
			//Silence new channels
			vector<float> newSoundData( this->getSampleFrameCount() * newChannelCount );
			for (unsigned int sample = 0; sample < getSampleFrameCount(); sample++) {
				for (unsigned int oldChannel = 0; oldChannel < _soundChannels; oldChannel++) {
					newSoundData[ (sample * newChannelCount) + oldChannel ] = _soundData[ (sample * _soundChannels) + oldChannel ];
				}
				for (unsigned int newChannel = _soundChannels; newChannel < newChannelCount; newChannel++) {
					newSoundData[ (sample * newChannelCount) + newChannel ] = 0;
				}
			}
		}

		_soundChannels = newChannelCount;
		_soundData = newSoundData;

		return true;
	}
	
	if (N < O) {
		std::vector<float> newSoundData( this->getSampleFrameCount() * newChannelCount );

		if (average) {
			//the data from the `O - N` to-be-removed channels are averaged and added on to the `N` remaining channels
			
			float removed = float(O - N);
			//Scaling factors
			float sigma = (float)N / (N + removed);
			float gamma = 1.0 / N;

			for (unsigned int sampleFrame = 0; sampleFrame < this->getSampleFrameCount(); sampleFrame++) {

				//Get the sum based on the to be removed channels
				float sum = 0;
				for (unsigned int oldChannel = O - 1; oldChannel >= N; oldChannel--) {
					sum += _soundData[ (sampleFrame * _soundChannels) + oldChannel ];
				}

				//Add the average of the old data to the remaining channels, maintaining equal ratios
				for (unsigned int keptChannel = 0; keptChannel < N; keptChannel++) {
					float samp = _soundData[ (sampleFrame * _soundChannels) + keptChannel ];
					newSoundData[ (sampleFrame * N) + keptChannel ] = (samp + (sum * gamma)) * sigma;
				}
			}

		} else {
			//the data from the `O - N` to-be-removed channels is discarded.
			for (unsigned int sampleFrame = 0; sampleFrame < this->getSampleFrameCount(); sampleFrame++) {
				for (unsigned int retainedChannel = 0; retainedChannel < N; retainedChannel++) {
					newSoundData[ (sampleFrame * N) + retainedChannel ] = _soundData[ (sampleFrame * _soundChannels) + retainedChannel ];
				}
			}
		}

		_soundChannels = newChannelCount;
		_soundData = newSoundData;

		return true;
	}
	/*
	CX::Instances::Log.error("CX_SoundBuffer") << "Sound cannot be set to the given number of channels. There is no known conversion from " <<
						_soundChannels << " channels to " << newChannelCount << 
						" channels. You will have to do it manually. Use getRawDataReference() to access the sound data." << endl;

	return false;
	*/
}

/*!
Resamples the audio data stored in the CX_SoundBuffer by linear interpolation. Linear interpolation is not the ideal
way to resample audio data; some audio fidelity is lost, more so than with other resampling techinques. It is, however, 
very fast compared to higher-quality methods both in terms of run time and programming time. It has acceptable results, 
at least when the new sample rate is similar to the old sample rate.

\param newSampleRate The requested sample rate.
*/
void CX_SoundBuffer::resample (float newSampleRate) {
	
	uint64_t oldSampleCount = getSampleFrameCount();
	uint64_t newSampleCount = (uint64_t)(getSampleFrameCount() * ((double)newSampleRate / _soundSampleRate));

	vector<float> completeNewData((unsigned int)newSampleCount * _soundChannels);

	for (int channel = 0; channel < _soundChannels; channel++) {

		for (unsigned int sample = 0; sample < newSampleCount; sample++) {
			double time = ((double)sample)/newSampleCount;

			double oldIndex = time * oldSampleCount;
			double oldIndexOffset = fmod( oldIndex, 1.0 );

			unsigned int i1 = (unsigned int)floor(oldIndex);
			unsigned int i2 = i1 + 1;

			//There is a little fudge at the end. If the last sample would be past the end of the old data, reuse the
			//previous sample. This is technically an error, but at the last sample it can't possibly have a meaningful effect.
			if (i2 >= oldSampleCount) {
				i2 = i1;
			}

			float s1 = _soundData[ (i1 * _soundChannels) + channel ];
			float s2 = _soundData[ (i2 * _soundChannels) + channel ];
			float linInt = s1 + (s2 - s1)*(float)oldIndexOffset;

			completeNewData[(sample * _soundChannels) + channel] = linInt;
		}
	}

	_soundData = completeNewData;

	_soundSampleRate = newSampleRate;

}

/*! This function reverses the sound data stored in the CX_SoundBuffer so that if it is played, it will
play in reverse. */
void CX_SoundBuffer::reverse(void) {
	vector<float> copy = _soundData;
	unsigned int sampleFrameCount = getSampleFrameCount();
	for (unsigned int sf = 0; sf < sampleFrameCount; sf++) {
		unsigned int targetSampleFrame = sf*_soundChannels;
		unsigned int sourceSampleFrame = (sampleFrameCount - 1 - sf) * _soundChannels;

		for (unsigned int ch = 0; ch < _soundChannels; ch++) {
			_soundData[targetSampleFrame + ch] = copy[sourceSampleFrame + ch];
		}
	}
}

/*! This function changes the speed of the sound by some multiple.
\param speedMultiplier Amount to multiply the speed by. Must be greater than 0.
\note If you would like to use a negative value to reverse the direction of playback, see reverse().
*/
void CX_SoundBuffer::multiplySpeed (float speedMultiplier) {
	if (speedMultiplier <= 0) {
		return;
	}

	float sampleRate = this->_soundSampleRate;
	this->resample( this->getSampleRate() / speedMultiplier );
	this->_soundSampleRate = sampleRate;
}

/*!
Apply gain to the channel in terms of decibels.
\param decibels Gain to apply. 0 does nothing. Positive values increase volume, negative values decrease volume. Negative infinity is essentially mute,
although see multiplyAmplitudeBy() for a more obvious way to do that same operation.
\param channel The channel that the gain should be applied to. If channel is less than 0, the gain is applied to all channels.
*/
bool CX_SoundBuffer::applyGain (float decibels, int channel) {
	float amplitudeMultiplier = sqrt( pow(10.0f, decibels/10.0f) );
	return multiplyAmplitudeBy( amplitudeMultiplier, channel );
}

/*!
Apply gain to the sound. The original value is simply multiplied by the amount and then clamped to be within [-1, 1].
\param amount The gain that should be applied. A value of 0 mutes the channel. 1 does nothing. 2 doubles the amplitude. -1 inverts the waveform.
\param channel The channel that the given multiplier should be applied to. If channel is less than 0, the amplitude multiplier is applied to all channels.
*/
bool CX_SoundBuffer::multiplyAmplitudeBy (float amount, int channel) {

	if (channel >= _soundChannels) {
		return false;
	}

	if (channel < 0) {
		//Apply to all channels
		for (unsigned int i = 0; i < _soundData.size(); i++) {
			_soundData[i] = Util::clamp<float>(_soundData[i] * amount, -1, 1);
		}

	} else {
		//Apply gain to the given channel
		for (unsigned int sf = 0; sf < (unsigned int)getSampleFrameCount(); sf++) {
			unsigned int index = (sf * _soundChannels) + channel;
			_soundData[index] = Util::clamp<float>(_soundData[index] * amount, -1, 1);
		}

	}
	return true;
}

/*! Clears all data stored in the sound buffer and returns it to an uninitialized state. */
void CX_SoundBuffer::clear(void) {
	_soundData.clear();
	_successfullyLoaded = false;
	_soundChannels = 0;
	_soundSampleRate = 0;
}


/*! Writes the contents of the sound buffer to a file with the given file name. The data will
be encoded as 16-bit PCM. The sample rate is determined by the sample rate of the sound buffer.
\param filename The name of the file to save the sound data to. `filename` should have a .wav extension. If it does not,
".wav" will be appended to the file name and a warning will be logged.
\return False if there was an error while opening the file. If so, an error will be logged.
*/
bool CX_SoundBuffer::writeToFile(std::string filename) {
	//This function was taken from the ofSoundFile additions suggested here: https://github.com/openframeworks/openFrameworks/pull/2626
	//From this file: https://github.com/admsyn/openFrameworks/blob/feature-sound-objects/libs/openFrameworks/sound/ofSoundFile.cpp
	//There were some modifications to get it to work with the data structure of CX_SoundBuffer.

	// check that we're writing a wav and complain if the file extension is wrong.
	ofFile f(filename);
	if (ofToLower(f.getExtension()) != "wav") {
		filename += ".wav";
		CX::Instances::Log.warning("CX_SoundBuffer") << "writeToFile: Can only write wav files - will save file as " << filename;
	}

	fstream file(ofToDataPath(filename).c_str(), ios::out | ios::binary);
	if (!file.is_open()) {
		CX::Instances::Log.error("CX_SoundBuffer") << "writeToFile: Error opening sound file '" << filename << "' for writing.";
		return false;
	}

	// write a wav header
	short myFormat = 1; // for pcm
	int mySubChunk1Size = 16;
	int bitsPerSample = 16; // assume 16 bit pcm

	int channels = this->getChannelCount();
	int samplerate = this->getSampleRate();
	unsigned int bufferSize = this->getTotalSampleCount();

	int myByteRate = samplerate * channels * bitsPerSample / 8;
	short myBlockAlign = channels * bitsPerSample / 8;
	int myChunkSize = 36 + bufferSize*bitsPerSample / 8;
	int myDataSize = bufferSize*bitsPerSample / 8;


	file.seekp(0, ios::beg);
	file.write("RIFF", 4);
	file.write((char*)&myChunkSize, 4);
	file.write("WAVE", 4);
	file.write("fmt ", 4);
	file.write((char*)&mySubChunk1Size, 4);
	file.write((char*)&myFormat, 2); // should be 1 for PCM
	file.write((char*)&channels, 2); // # channels (1 or 2)
	file.write((char*)&samplerate, 4); // 44100
	file.write((char*)&myByteRate, 4); //
	file.write((char*)&myBlockAlign, 2);
	file.write((char*)&bitsPerSample, 2); //16
	file.write("data", 4);
	file.write((char*)&myDataSize, 4);

	// write the wav file per the wav file format, 4096 bytes of data at a time.
#define WRITE_BUFF_SIZE 4096

	short writeBuff[WRITE_BUFF_SIZE];
	int pos = 0;
	while (pos < bufferSize) {
		int len = MIN(WRITE_BUFF_SIZE, bufferSize - pos);
		for (int i = 0; i < len; i++) {
			writeBuff[i] = (int)(this->_soundData[pos] * 32767.f);
			pos++;
		}
		file.write((char*)writeBuff, len*bitsPerSample / 8);
	}

	file.close();
	return true;
}



/*
float CX_SoundBuffer::_readSample (int channel, unsigned int sample) {
	unsigned int newIndex = (sample * _soundChannels) + channel;
	return _soundData.at( newIndex );
}

vector<float> CX_SoundBuffer::_getChannelData (int channel) {
	vector<float> rval( (unsigned int)getSampleFrameCount() );

	for (unsigned int sample = 0; sample < rval.size(); sample++) {
		rval[sample] = _soundData.at( (sample * _soundChannels) + channel );
	}

	return rval;
}
*/

} //namespace CX