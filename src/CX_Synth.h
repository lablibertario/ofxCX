#pragma once

#include "ofEvents.h"
#include "CX_SoundStream.h"
#include "CX_SoundBuffer.h"

#include "CX_RandomNumberGenerator.h" //For white noise generator
#include "CX_Time_t.h"

/*! \namespace CX::Synth
This namespace contains a number of classes that can be combined together to form a modular
synthesizer that can be used to procedurally generate sound stimuli. There are methods for
saving the sound stimuli to a file for later use or directly outputting the sounds to sound
hardware. There is also a way to use the data from a CX_SoundBuffer as the input to the synth.

There are two types of oscillators (Oscillator and AdditiveSynth), an ADSR Envelope, two types
of filters (Filter and FIRFilter), a Splitter and a Mixer, and some utility classes for adding,
multiplying, and clamping values.

Making your own modules is simplified by the fact that all modules inherit from ModuleBase. You
only need to overload one function from ModuleBase in order to have a functional module, although
there are some other functions that can be overloaded for advanced uses.

\ingroup sound
*/

namespace CX {
namespace Synth {

	double sinc(double x);
	double relativeFrequency(double f, double semitoneDifference);

	struct ModuleControlData_t {
		ModuleControlData_t(void) :
			initialized(false),
			sampleRate(666),
			oversampling(1)
		{}

		ModuleControlData_t(float sampleRate_) :
			initialized(true),
			sampleRate(sampleRate_),
			oversampling(1)
		{}

		bool initialized;
		float sampleRate;
		unsigned int oversampling;

		bool operator==(const ModuleControlData_t& right) {
			return ((this->initialized == right.initialized) && (this->sampleRate == right.sampleRate) && (this->oversampling == right.oversampling));
		}

		bool operator!=(const ModuleControlData_t& right) {
			return !(this->operator==(right));
		}
	};

	class ModuleParameter;

	/*! All modules of the modular synth inherit from this class.
	\ingroup modSynth */
	class ModuleBase {
	public:

		ModuleBase(void);
		~ModuleBase(void);

		virtual double getNextSample(void);

		void setData(ModuleControlData_t d);
		ModuleControlData_t getData(void);

		void disconnectInput(ModuleBase* in);
		void disconnectOutput(ModuleBase* out);

		void disconnect(void);

	protected:

		friend ModuleBase& operator>>(ModuleBase& l, ModuleBase& r);
		friend void operator>>(ModuleBase& l, ModuleParameter& r);

		std::vector<ModuleBase*> _inputs; //!< The inputs to this module.
		std::vector<ModuleBase*> _outputs; //!< The outputs from this module.
		std::vector<ModuleParameter*> _parameters; //!< The ModuleParameters of this module.
		ModuleControlData_t *_data; //!< The data for this module.

		void _dataSet(ModuleBase* caller);
		void _setDataIfNotSet(ModuleBase* target);
		void _registerParameter(ModuleParameter* p);

		void _assignInput(ModuleBase* in);
		void _assignOutput(ModuleBase* out);

		//Feel free to overload any of the virtual functions in dervied modules.

		virtual void _dataSetEvent(void);

		//The values returned by these functions directly control how many inputs or outputs a modules can have at once.
		//If more modules are assigned as input or outputs than are allowed, previously assigned inputs or outputs are
		//disconnected.
		virtual unsigned int _maxInputs(void);
		virtual unsigned int _maxOutputs(void);

		//These functions are called whenever an input or output has been assigned to this module.
		virtual void _inputAssignedEvent(ModuleBase* in);
		virtual void _outputAssignedEvent(ModuleBase* out);
	};

	/*! This class is used to provide modules with the ability to have their control parameters change as a
	function of incoming data from other modules. For example, if you want to change the frequency of an
	oscillator, you can feed an LFO into the frequency parameter of the oscillator.

	If you create a module that uses a ModuleParameter, you must perform one setup step in the constructor of the
	module. You must call ModuleBase::_registerParameter() with the ModuleParameter as the argument.
	\ingroup modSynth
	*/
	class ModuleParameter {
	public:

		ModuleParameter(void);
		ModuleParameter(double d);

		void updateValue(void);
		bool valueUpdated(bool checkForUpdates = true);
		double& getValue(void);

		operator double(void);

		ModuleParameter& operator=(double d);

		friend void operator>>(ModuleBase& l, ModuleParameter& r);

	private:
		friend class ModuleBase;

		ModuleBase* _owner; // A pointer to the module that this ModuleParameter is owned by.
		ModuleBase* _input; // The input to the parameter. Parameters have one input and no outputs.

		bool _updated;
		double _value;
	};


	ModuleBase& operator>>(ModuleBase& l, ModuleBase& r);
	void operator>>(ModuleBase& l, ModuleParameter& r);

	/*! This class is an implementation of an additive synthesizer. Additive synthesizers are essentially an
	inverse fourier transform. You specify at which frequencies you want to have a sine wave and the amplitudes
	of those waves, and they are combined together into a single waveform.

	The frequencies are refered to as harmonics, due to the fact that typical audio applications of additive
	synths use the standard harmonic series (f(i) = f_fundamental * i). However, setting the harmonics to
	values not found in the standard harmonic series can result in really unusual and interesting sounds.

	The output of the additive synth is not easily bounded between -1 and 1 due to various oddities of
	additive synthesis. For example, although in the limit as the number of harmonics goes to infinity
	square and sawtooth waves made with additive synthesis are bounded between -1 and 1, with smaller
	numbers of harmonics the amplitudes actually overshoot these bounds slightly. Of course, if an unusual
	harmonic series is used with arbitrary amplitudes, it can be hard to know if the output of the synth
	will be within the bounds. A Synth::Multiplier can help deal with this.
	\ingroup modSynth
	*/
	class AdditiveSynth : public ModuleBase {
	public:

		typedef double amplitude_t; //!< A floating-point type used for the waveform amplitudes.
		typedef double frequency_t; //!< A floating-point type used for the frequencies of the waves.

		/*! The type of function that will be used to create the harmonic series for the additive synth. */
		enum class HarmonicSeriesType {
			MULTIPLE, //Includes the standard harmonic series as a special case
			SEMITONE //Includes all of the strange thirds, fourths, tritones, etc.
		};

		/*! Assuming that the standard harmonic series is being used, the values in this
		enum, when passed to setAmplitudes(), cause the amplitudes of the harmonics to be
		set in such a way as to produce the desired waveform. */
		enum class AmplitudePresets {
			SINE,
			SQUARE,
			SAW,
			TRIANGLE
		};

		AdditiveSynth(void);

		ModuleParameter fundamental; //!< The fundamental frequency (the first harmonic) of the synth.

		void setStandardHarmonicSeries(unsigned int harmonicCount);
		void setHarmonicSeries (unsigned int harmonicCount, HarmonicSeriesType type, double controlParameter);
		void setHarmonicSeries(std::vector<frequency_t> harmonicSeries);

		void setAmplitudes(AmplitudePresets a);
		void setAmplitudes(AmplitudePresets a1, AmplitudePresets a2, double mixture);
		void setAmplitudes(std::vector<amplitude_t> amps);
		std::vector<amplitude_t> calculateAmplitudes(AmplitudePresets a, unsigned int count);

		void pruneLowAmplitudeHarmonics(double tol);

		double getNextSample(void) override;

	private:

		struct HarmonicInfo {
			HarmonicInfo(void) :
				relativeFrequency(1),
				amplitude(0)
			{}

			//set
			frequency_t relativeFrequency;
			amplitude_t amplitude;

			//calculated
			double positionChangePerSample;

			//updated
			double waveformPosition;
		};

		std::vector<HarmonicInfo> _harmonics;

		void _recalculateWaveformPositions(void);

		void _dataSetEvent(void) override;

	};

	/*! This class simply takes an input and adds an `amount` to it. The `amount` can be negative, in which case this
	class is a subtracter. If there is no input to this module, it behaves as though the input is 0, so the output
	value will be equal to `amount`. Thus, it can also behave as a numerical constant.
	\ingroup modSynth
	*/
	class Adder : public ModuleBase {
	public:
		Adder(void);
		double getNextSample(void) override;

		ModuleParameter amount; //!< The amount that will be added to the input signal.
	};

	/*! This class clamps inputs to be in the interval [`low`, `high`], where `low` and `high` are the members of this class.
	\ingroup modSynth
	*/
	class Clamper : public ModuleBase {
	public:
		Clamper(void);

		double getNextSample(void) override;

		ModuleParameter low; //!< The lowest possible output value.
		ModuleParameter high; //!< The highest possible output value.
	};

	/*! This class is a standard ADSR envelope: http://en.wikipedia.org/wiki/Synthesizer#ADSR_envelope.
	`s` should be in the interval [0,1]. `a`, `d`, and `r` are expressed in seconds.
	Call attack() to start the envelope. Once the attack and decay are finished, the envelope will
	stay at the sustain level until release() is called.

	The output values produced start at 0, rise to 1 during the attack, drop to the sustain level (`s`) during
	the decay, and drop from `s` to 0 during the release.

	\ingroup modSynth
	*/
	class Envelope : public ModuleBase {
	public:

		Envelope(void);

		double getNextSample(void) override;

		void attack(void);
		void release(void);

		/*! This parameter can be used by another module as a way to signal the Envelope. When the output of the module inputting
		to `gateInput` changes to 1.0, the attack of the envolpe is triggered. When it changes to 0, the release is triggered. */
		ModuleParameter gateInput;

		ModuleParameter a; //!< The number of seconds it takes, following the attack, for the level to rise from 0 to 1. Should be non-negative.
		ModuleParameter d; //!< The number of seconds it takes, once reaching the attack peak, to fall to `s`. Should be non-negative.
		ModuleParameter s; //!< The level at which the envelope sustains while waiting for the release. Should be between 0 and 1.
		ModuleParameter r; //!< The number of seconds it takes, following the release, for the level to fall to 0 from `s`. Should be non-negative.

	private:
		int _stage;

		double _lastP;
		double _levelAtRelease;

		double _timePerSample;
		double _timeSinceLastStage;

		void _dataSetEvent(void);

		double _a;
		double _d;
		double _s;
		double _r;
	};


	/*!	This class provides a basic way to filter waveforms as part of subtractive synthesis or
	other audio manipulation.

	This class is based on simple IIR filters. They may not be stable at all frequencies.
	They are computationally very efficient. They are not highly configurable. They may be chained
	for sharper frequency response.	This class is based on this chapter: http://www.dspguide.com/ch19.htm.
	\ingroup modSynth */
	class Filter : public ModuleBase {
	public:

		/*! The type of filter to use. */
		enum FilterType {
			LOW_PASS,
			HIGH_PASS,
			BAND_PASS,
			NOTCH
		};

		Filter(void);

		void setType(FilterType type);

		double getNextSample(void) override;

		/*! The cutoff frequency of the filter. */
		ModuleParameter cutoff;

		/*! Only used for BAND_PASS and NOTCH FilterTypes. Sets the width (in frequency domain) of the stop or pass band
		at which the amplitude is equal to sin(PI/4) (i.e. .707). So, for example, if you wanted the frequencies
		100 Hz above and below the breakpoint to be at .707 of the maximum amplitude, set `bandwidth` to 100.
		Of course, past those frequencies the attenuation continues.
		Larger values result in a less pointy band.	*/
		ModuleParameter bandwidth;

	private:

		FilterType _filterType;

		void _dataSetEvent(void) override {
			_recalculateCoefficients();
		}

		void _recalculateCoefficients(void);

		double a0;
		double a1;
		double a2;
		double b1;
		double b2;

		double x1;
		double x2;
		double y1;
		double y2;
	};

	/*! This class is an easy way to apply an arbitrary function to modular synth data.
	The user function, `f`, takes a `double` and returns a `double`. Each time getNextSample()
	is called, the next sample from the input to this module will be taken and passed to `f`,
	and the the result of `f` will be returned.
	*/
	class FunctionModule : public ModuleBase {
	public:
		double getNextSample(void) override {
			double v = 0;
			if (_inputs.size() >= 1) {
				v = _inputs.front()->getNextSample();
			}
			return f(v);
		}

		std::function<double(double)> f; //!< The user function, which will be called each time getNextSample() is called.
	};

	/*! This class is used within output modules that actually output data. This class serves as
	an endpoint for data that is then retrieved by the class containing the GenericOutput. See, for
	example, the StereoStreamOutput class. This class does nothing useful on its own (getNextSample()
	is just a passthrough).

	\ingroup modSynth */
	class GenericOutput : public ModuleBase {
	public:
		double getNextSample(void) override {
			if (_inputs.size() == 0) {
				return 0;
			}
			double sum = 0;
			for (unsigned int i = 0; i < _data->oversampling; i++) {
				sum += _inputs.front()->getNextSample();
			}
			return sum / _data->oversampling;
		}
	private:
		unsigned int _maxOutputs(void) override { return 0; };
		void _inputAssignedEvent(ModuleBase* in) override {
			in->setData(this->getData());
		}
	};

	/*! This class mixes together a number of inputs. It does no mixing in the usual sense of
	setting levels of the inputs, which is done with Multipliers. This class simply
	adds together all of the inputs with no amplitude correction, so it is possible for the
	output of the mixer to have very large amplitudes.

	This class is special in that it can have more than one input.

	\ingroup modSynth
	*/
	class Mixer : public ModuleBase {
	public:
		double getNextSample(void) override;
	private:
		unsigned int _maxInputs(void) override;
	};

	/*! This class multiplies an input by an `amount`. You can set the amount in terms of decibels
	of gain by using the setGain() function. If there is no input to this module, it behaves as though
	the input was 0 and consequently outputs 0.
	\ingroup modSynth
	*/
	class Multiplier : public ModuleBase {
	public:
		Multiplier(void);
		Multiplier(double amount);

		double getNextSample(void) override;
		void setGain(double decibels);

		ModuleParameter amount; //!< The amount that the input signal will be multiplied by.
	};

	/*! This class provides one of the simplest ways of generating waveforms. The output
	from an Oscillator can be filtered with a CX::Synth::Filter or used in other ways.

	\code{.cpp}
	using namespace CX::Synth;
	//Configure the oscillator to produce a square wave with a fundamental frequency of 200 Hz.
	Oscillator osc;
	osc.frequency = 200; //200 Hz
	osc.setGeneratorFunction(Oscillator::square); //Produce a square wave
	\endcode
	\ingroup modSynth
	*/
	class Oscillator : public ModuleBase {
	public:

		Oscillator(void);

		double getNextSample(void) override;

		void setGeneratorFunction(std::function<double(double)> f);

		ModuleParameter frequency; //!< The fundamental frequency of the oscillator.

		static double saw(double wp);
		static double sine(double wp);
		static double square(double wp);
		static double triangle(double wp);
		static double whiteNoise(double wp);

	private:
		std::function<double(double)> _generatorFunction;
		float _frequencyDivisor;
		//float _sampleRate; //This is a slight optimization. This just needs to refer to a data member, rather than _data->sampleRate.
		double _waveformPos;

		void _dataSetEvent(void);

		unsigned int _maxInputs(void) override { return 0; };
	};


	/*! This class is an implementation of a very basic ring modulator.	Ringmods need two inputs: 
	the source and the carrier. The order doesn't matter, for this class. If only one input is 
	given, it will just pass that input through.

	This is not an analog emulation and it does nothing to deal with aliasing, so it may not work well
	with non-sinusoidal carriers.

	\code{.cpp}

	StreamInput input;
	input.setup(&ss); //Assume that ss is a CX_SoundStream that is configured for input.

	StreamOutput output;
	output.setup(&ss); //Assume that ss is also configured for output.

	Oscillator carrier;
	carrier.setGeneratorFunction(Oscillator::sine);
	carrier.frequency = 250;

	RingModulator rm;

	carrier >> rm; //Connect the carrier
	input >> rm; //And the source

	Multiplier m(0.1);

	rm >> m >> output;

	\endcode
	*/
	class RingModulator : public ModuleBase {
	public:
		double getNextSample(void) override;
	private:
		unsigned int _maxInputs(void) override;
	};


	/*! This class splits a signal and sends that signal to multiple outputs. This can be used
	for panning effects, for example.

	This class is special because it allows multiple outputs.

	\code{.cpp}
	using namespace CX::Synth;

	Splitter sp;
	Oscillator osc;
	Multiplier m1;
	Multiplier m2;
	StereoStreamOutput out;

	//In runExperiment:
	osc >> sp;
	sp >> m1 >> out.left;
	sp >> m2 >> out.right;
	\endcode
	\ingroup modSynth
	*/
	class Splitter : public ModuleBase {
	public:
		Splitter(void);
		double getNextSample(void) override;

	private:
		void _outputAssignedEvent(ModuleBase* out) override;
		unsigned int _maxOutputs(void) override { return 32; };

		double _currentSample;
		unsigned int _fedOutputs;
	};

	/*! This class allows you to use a CX_SoundBuffer as the input for the modular synth.
	It is strictly monophonic, so when you associate a CX_SoundBuffer with this class,
	you must pick one channel of the sound to use. You can use multiple SoundBufferInputs
	to play multiple channels from the same CX_SoundBuffer.
	\ingroup modSynth
	*/
	class SoundBufferInput : public ModuleBase {
	public:
		SoundBufferInput(void);

		double getNextSample(void) override;

		void setSoundBuffer(CX::CX_SoundBuffer *sb, unsigned int channel = 0);
		void setTime(CX_Millis t);
		bool canPlay(void);

	private:
		CX::CX_SoundBuffer *_sb;
		unsigned int _channel;
		unsigned int _currentSample;

	};


	/*! This class is a module that takes input from a CX_SoundStream configured for input, so
	it is good for getting sounds from a microphone or line in. This class is strictly monophonic.

	In order to be compatible with the other modules, this module takes in sound data and stores
	it in an internal buffer. Requests for samples from this class will takes samples from the
	buffer. If the buffer is empty, this will output 0. If there are no requests for samples from
	this class for a long time, its buffer can get very large. Then, when samples are requested,
	the samples it gives out will be very old. For this reason, user code can configure a maximum
	buffer size using setMaximumBufferSize(). The maximum buffer size defaults to 4096 samples.
	User code can clear the buffer with clear().
	*/
	class StreamInput : public ModuleBase {
	public:
		StreamInput(void);
		~StreamInput(void);

		void setup(CX::CX_SoundStream* stream);

		double getNextSample(void) override;

		void clear(void);
		void setMaximumBufferSize(unsigned int size);
	private:

		unsigned int _maxBufferSize;
		std::deque<float> _buffer;

		CX::CX_SoundStream* _soundStream;
		bool _listeningForEvents;

		void _callback(CX::CX_SoundStream::InputEventArgs& in);
		void _listenForEvents(bool listen);
		unsigned int _maxInputs(void) override;
	};

	/*! This class provides a method of playing the output of a modular synth using a CX_SoundStream.
	This class can only take data from one input, so it is monophonic. However, the sound stream
	does not need to be configured to only use 1 output channel because this class will put
	the same data on all available output channels.
	In order to use this class, you need to configure a CX_SoundStream for use. See the soundBuffer
	example and the CX::CX_SoundStream class for more information.

	\code{.cpp}
	using namespace CX::Synth;
	//Assume that both osc and ss have been configured and that ss has been started.
	CX_SoundStream ss;
	Oscillator osc;

	Synth::StreamOutput output;
	output.setup(&ss);

	osc >> output; //Sound should be playing past this point.
	\endcode

	\ingroup modSynth
	*/
	class StreamOutput : public ModuleBase {
	public:
		~StreamOutput(void);
		void setup(CX::CX_SoundStream* stream);

	private:
		void _callback(CX::CX_SoundStream::OutputEventArgs& d);
		unsigned int _maxOutputs(void) override { return 0; };
		void _inputAssignedEvent(ModuleBase* in) override {
			in->setData(this->getData());
		}

		CX_SoundStream* _soundStream;
		bool _listeningForEvents;
		void _listenForEvents(bool listen);
	};

	/*! This class is much like StreamOutput except in stereo. This captures stereo audio by taking the
	output of different streams of data into either the `left` or `right` modules that this class has.
	See the example code for CX::Synth::StereoSoundBufferOutput and CX::Synth::StreamOutput for ideas
	on how to use this class.
	\ingroup modSynth
	*/
	class StereoStreamOutput {
	public:

		~StereoStreamOutput(void);

		void setup(CX::CX_SoundStream* stream);

		GenericOutput left; //!< The left channel of the stream.
		GenericOutput right; //!< The right channel of the stream.

	private:
		void _callback(CX::CX_SoundStream::OutputEventArgs& d);

		CX_SoundStream* _soundStream;
		bool _listeningForEvents;
		void _listenForEvents(bool listen);
	};

	/*! This class provides a method of capturing the output of a modular synth and storing it in a CX_SoundBuffer
	for later use. See the documentation for CX::Synth::StereoSoundBufferOutput to get an idea of how to use this class.
	\ingroup modSynth
	*/
	class SoundBufferOutput : public ModuleBase {
	public:
		void setup(float sampleRate);
		void sampleData(CX_Millis t);

		CX::CX_SoundBuffer sb; //!< The sound buffer that will be filled with samples with sampleData() is called.
	private:
		unsigned int _maxOutputs(void) override { return 0; };
	};


	/*! This class provides a method of capturing the output of a modular synth and storing it in a CX_SoundBuffer
	for later use. This captures stereo audio by taking the output of different streams of data into either the
	`left` or `right` modules that this class has. See the example code.

	\code{.cpp}
	#include "CX.h"

	using namespace CX::Synth;

	void runExperiment(void) {
		StereoSoundBufferOutput sout;
		sout.setup(44100);

		Splitter sp;
		Oscillator osc;
		Multiplier leftM;
		Multiplier rightM;

		osc.frequency = 400;
		leftM.amount = .1;
		rightM.amount = .01;

		osc >> sp;
		sp >> leftM >> sout.left;
		sp >> rightM >> sout.right;

		sout.sampleData(CX_Seconds(2)); //Sample 2 seconds worth of data on both channels.
		sout.sb.writeToFile("Stereo.wav");
	}
	\endcode

	\ingroup modSynth */
	class StereoSoundBufferOutput {
	public:
		void setup(float sampleRate);
		void sampleData(CX_Millis t);

		GenericOutput left; //!< The left channel of the buffer.
		GenericOutput right; //!< The right channel of the buffer.

		CX::CX_SoundBuffer sb; //!< The sound buffer that will be filled with samples with sampleData() is called.
	};


	/*! This class is used for numerically, rather than auditorily, testing other modules.
	It produces samples starting at `value` and increasing by `step`. */
	class TrivialGenerator : public ModuleBase {
	public:

		TrivialGenerator(void);

		ModuleParameter value; //!< The start value.
		ModuleParameter step; //!< The amount to change on each step.

		double getNextSample(void) override;
	};


	/*! This class is a start at implementing a Finite Impulse Response filter (http://en.wikipedia.org/wiki/Finite_impulse_response).
	You can use it as a basic low-pass or high-pass	filter, or, if you supply your own coefficients, which cause the
	filter to do filtering in whatever way you want. See the "signal" package for R for a method of constructing your own coefficients.
	\ingroup modSynth
	*/
	class FIRFilter : public ModuleBase{
	public:

		/*! The type of filter to use. */
		enum class FilterType {
			LOW_PASS,
			HIGH_PASS,
			BAND_PASS,
			BAND_STOP,
			USER_DEFINED //!< Should not be used directly.
		};

		/*! The type of windowing function to apply after convolution. */
		enum WindowType {
			RECTANGULAR,
			HANNING,
			BLACKMAN
		};

		FIRFilter(void);

		void setup(FilterType filterType, unsigned int coefficientCount);
		void setup(std::vector<double> coefficients);

		void setCutoff(double cutoff);
		void setBandCutoffs(double lower, double upper);

		double getNextSample(void);

	private:

		FilterType _filterType;
		WindowType _windowType;

		int _coefCount;

		std::vector<double> _coefficients;
		std::deque<double> _inputSamples;

		double _calcH(int n, double omega);

		void _applyWindowToCoefs(void);

	};

} //namespace Synth
} //namespace CX
