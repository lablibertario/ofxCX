#pragma once

#include "CX_SoundStream.h"
#include "CX_SoundBuffer.h"

namespace CX {
	/*! This class is used for recording audio data from, e.g., a microphone. The recorded data is
	stored in a CX_SoundBuffer for further use. 
	
	\code{.cpp}
	CX_SoundBufferRecorder recorder;

	CX_SoundBufferRecorder::Configuration recorderConfig;
	recorderConfig.inputChannels = 1;
	//You will probably need to configure more than just the number of input channels.
	recorder.setup(recorderConfig);

	CX_SoundBuffer recording;
	recorder.setSoundBuffer(&recording); //Associate a CX_SoundBuffer with the recorder so that the buffer can be recorded to.

	//Record for 5 seconds
	recorder.start();
	Clock.sleep(CX_Seconds(5));
	recorder.stop();

	//Write the recording to a file
	recording.writeToFile("recording.wav");
	\endcode
	\ingroup sound
	*/
	class CX_SoundBufferRecorder {
	public:
		typedef CX_SoundStream::Configuration Configuration; //!< This is typedef'ed to \ref CX::CX_SoundStream::Configuration.

		CX_SoundBufferRecorder(void);
		~CX_SoundBufferRecorder(void);

		bool setup(Configuration& config);
		bool setup(CX_SoundStream* ss);

		Configuration getConfiguration(void);

		//! Provides direct access to the CX_SoundStream used by the CX_SoundBufferRecorder.
		CX_SoundStream* getSoundStream(void) { return _soundStream; };

		void setSoundBuffer(CX_SoundBuffer* soundBuffer);
		CX_SoundBuffer* getSoundBuffer(void);

		void start(bool clearExistingData = false);
		void stop(void);

		bool isRecording(void) const;

	private:
		bool _inputEventHandler(CX_SoundStream::InputEventArgs& inputData);

		bool _recording;

		CX_SoundBuffer *_buffer;

		CX_SoundStream *_soundStream;
		bool _soundStreamSelfAllocated;
		void _cleanUpOldSoundStream(void);

		void _listenForEvents(bool listen);
		bool _listeningForEvents;
	};

}