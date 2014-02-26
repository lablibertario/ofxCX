#include "CX_EntryPoint.h"

#include "CX_ModularSynth.h"

CX_SoundStream ss;


void drawInformation(void);


void runExperiment(void) {
	CX_SoundStreamConfiguration_t config;
	config.api = RtAudio::Api::WINDOWS_DS;
	config.outputChannels = 2;
	config.sampleRate = 48000;
	config.bufferSize = 256;
	config.streamOptions.numberOfBuffers = 4;
	ss.setup(config);

	Oscillator osc;
	osc.frequency = 1000;
	osc.setGeneratorFunction(Oscillator::saw);

	Oscillator lfo;
	lfo.setGeneratorFunction(Oscillator::sine);
	lfo.frequency = 1;

	Adder add;

	Oscillator sawnOsc;
	sawnOsc.setGeneratorFunction(Oscillator::saw);
	
	Mixer oscMix;

	Multiplier mult;
	

	RCFilter f;

	Multiplier a;
	Multiplier sawnOscGain;
	a.amount = .01;
	sawnOscGain.amount = .01;

	Envelope en;
	en.a = 0;
	en.d = 0;
	en.s = 1;
	en.r = .2;
	
	StreamOutput output;
	output.setOuputStream(ss);
	
	Envelope modEnv;
	modEnv.a = .1;
	modEnv.d = .1;
	modEnv.s = 0;
	modEnv.r = .01;

	mult.amount = 3000;
	add.amount = 100;
	modEnv >> mult >> add >> f.breakpoint;

	osc >> a >> oscMix;
	sawnOsc >> sawnOscGain >> oscMix;

	oscMix >> en >> output;



	/*
	osc >> f >> a >> soOut;

	soOut.setup(44100);

	osc.setGeneratorFunction(Oscillator::sine);
	osc.frequency = 1500;
	f.setBreakpoint(10000);
	a.amount = 1;
	soOut.sampleData(.1);
	a.amount = 0;
	soOut.sampleData(.1);
	a.amount = 1;
	soOut.sampleData(.1);

	soOut.so.normalize(1);
	soOut.so.writeToFile("beep beep.wav");


	soOut.so.clear();
	osc.setGeneratorFunction(Oscillator::sine);
	osc.frequency = 600;
	f.setBreakpoint(10000);
	a.amount = 1;
	soOut.sampleData(.5);

	soOut.so.normalize(1);
	soOut.so.writeToFile("beep.wav");
	*/

	
	ss.start();

	Input.setup(true, true);

	drawInformation();

	while (1) {
		if (Input.pollEvents()) {
			while (Input.Mouse.availableEvents()) {
				CX_MouseEvent_t ev = Input.Mouse.getNextEvent();
				if (ev.eventType == CX_MouseEvent_t::MOVED || ev.eventType == CX_MouseEvent_t::DRAGGED) {
					osc.frequency = ev.x * 8 - 2;
					sawnOsc.frequency = ev.x * 8 + 2;
					cout << "F = " << osc.frequency.getValue() << endl;

					a.amount = (pow(Display.getResolution().y - ev.y, 1.5)) / (Display.getResolution().y * 10);
					sawnOscGain.amount = a.amount;
					cout << "A = " << a.amount.getValue() << endl;
					
					/*
					float smax = 0;
					for (int i = 0; i < 10; i++) {
						float t = a.getNextSample();
						if (t > smax) {
							smax = t;
						}
					}
					cout << "smax = " << smax << endl;
					*/
				}

				if (ev.eventType == CX_MouseEvent_t::PRESSED) {
					en.attack();
					modEnv.attack();
				}

				if (ev.eventType == CX_MouseEvent_t::RELEASED) {
					en.release();
					modEnv.release();
				}
			}

			while (Input.Keyboard.availableEvents()) {
				CX_KeyEvent_t ev = Input.Keyboard.getNextEvent();

				ss.hasSwappedSinceLastCheck();
				while (!ss.hasSwappedSinceLastCheck())
					;

				switch (ev.key) {
				case 't': osc.setGeneratorFunction(Oscillator::triangle); break;
				case 'q': osc.setGeneratorFunction(Oscillator::square); break;
				case 'i': osc.setGeneratorFunction(Oscillator::sine); break;
				case 'w': osc.setGeneratorFunction(Oscillator::saw); break;
				}
			}

			drawInformation();
		}

	}
}

void drawInformation(void) {
	Display.beginDrawingToBackBuffer();
	ofBackground(50);
	ofSetColor(255);
	ofDrawBitmapString("Low frequency", Display.getCenterOfDisplay() + ofPoint(-230, 0));
	ofDrawBitmapString("High frequency", Display.getCenterOfDisplay() + ofPoint(170, 0));
	ofDrawBitmapString("Low volume", Display.getCenterOfDisplay() + ofPoint(-30, 200));
	ofDrawBitmapString("High volume", Display.getCenterOfDisplay() + ofPoint(-30, -200));

	ofDrawBitmapString("Key: Waveform\nt: triangle\nq: square\ni: sine\nw: saw", Display.getCenterOfDisplay());

	Display.endDrawingToBackBuffer();
	Display.BLOCKING_swapFrontAndBackBuffers();
}