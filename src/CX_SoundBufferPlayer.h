#pragma once

/*! \defgroup sound Sound
There are a few different ways to deal with sounds in CX. The thing that most people want to do is to
play sounds, which is done with the CX_SoundBufferPlayer. See the soundBuffer tutorial for information
on how to do that.

If you want to record sound, use the CX_SoundBufferRecorder.

If you want to generate sound stimuli through sound synthesis, see the CX::Synth namespace and modularSynth
tutorial.

Finally, if you want to have direct control of the data going to and from a sound device, see CX_SoundStream.
*/

#include "CX_SoundBuffer.h"
#include "CX_SoundStream.h"
#include "CX_Clock.h"
#include "CX_Logger.h"

#include "ofEvents.h"

namespace CX  {


	/*!
	This class is used for playing CX_SoundBuffers. See the soundBuffer tutorial for an example of how to use this class.

	\ingroup sound
	*/
	class CX_SoundBufferPlayer {
	public:

		typedef CX_SoundStream::Configuration Configuration; //!< This is typedef'ed to \ref CX::CX_SoundStream::Configuration.

		CX_SoundBufferPlayer (void);
		~CX_SoundBufferPlayer (void);

		bool setup(Configuration config);
		bool setup(CX_SoundStream *ss);

		bool play (void);
		bool startPlayingAt(CX_Millis experimentTime, CX_Millis offset);
		bool stop (void);

		//! Check if the sound is currently playing.
		bool isPlaying (void) const { return _playing; };

		//! Check if the sound is queued to play.
		bool isQueuedToStart(void) const { return _playbackStartQueued; };

		Configuration getConfiguration(void);

		bool setSoundBuffer (CX_SoundBuffer *sound);
		CX_SoundBuffer* getSoundBuffer(void);

		/*! This function provides direct access to the CX_SoundStream used by the CX_SoundBufferPlayer. */
		CX_SoundStream* getSoundStream(void) { return _soundStream; };

		void seek(CX_Millis time);

	private:

		bool _outputEventHandler (CX_SoundStream::OutputEventArgs &outputData);

		CX_SoundStream *_soundStream;
		bool _soundStreamSelfAllocated;
		void _cleanUpOldSoundStream(void);

		void _listenForEvents(bool listen);
		bool _listeningForEvents;

		CX_SoundBuffer *_buffer;

		bool _withinOutputEvent;

		bool _playing;

		bool _playbackStartQueued;
		uint64_t _playbackStartSampleFrame;
		uint64_t _currentSampleFrame; //This is an absolute: It is never reset. At a sample rate of 48000 Hz, this will overflow every 12186300 years.
		uint64_t _soundPlaybackSampleFrame;	//This is relative to the current playback of the current sound buffer.
	};

}
