#include "CX_SlidePresenter.h"

using namespace CX;
using namespace CX::Instances;

CX_SlidePresenter::CX_SlidePresenter (void) :
	_display(nullptr),
	_presentingSlides(false),
	_synchronizing(false),
	_currentSlide(0),
	_lastFramebufferActive(false),
	_awaitingFenceSync(false),
	_deallocateFramebuffersForCompletedSlides(true),
	_errorMode(CX_SP_ErrorMode::PROPAGATE_DELAYS)
{}

bool CX_SlidePresenter::setup(CX_Display *display) {
	CX_SP_Configuration config;
	config.display = display;
	return setup(config);
}

bool CX_SlidePresenter::setup(const CX_SP_Configuration &config) {
	if (config.display != nullptr) {
		_display = config.display;
	} else {
		Log.error("CX_SlidePresenter") << "setDisplay: display is NULL.";
		return false;
	}

	_userFunction = config.userFunction;
	_deallocateFramebuffersForCompletedSlides = config.deallocateCompletedSlides;
	_errorMode = config.errorMode;
	return true;
}

/*
void CX_SlidePresenter::update (void) {

	if (_presentingSlides) {

		if (_display->hasSwappedSinceLastCheck()) {

			uint64_t currentFrameNumber = _display->getFrameNumber();

			//Was the current frame just swapped in? If so, store information about the swap time.
			if (_slides.at(_currentSlide).slideStatus == CX_Slide_t::SWAP_PENDING) {

				CX_Micros_t currentSlideOnset = _display->getLastSwapTime();

				_slides.at(_currentSlide).slideStatus = CX_Slide_t::IN_PROGRESS;
				_slides.at(_currentSlide).actualOnsetFrameNumber = currentFrameNumber;
				_slides.at(_currentSlide).actualSlideOnset = currentSlideOnset;
				

				//If on the first frame, some setup must be done for the rest of the frames. The first frame has just swapped in.
				if (_currentSlide == 0) {

					_slides.at(0).intendedOnsetFrameNumber = currentFrameNumber;
					_slides.at(0).intendedSlideOnset = currentSlideOnset; //This is sort of weird, but true.
					
					for (int i = 0; i < _slides.size() - 1; i++) {
						_slides.at(i + 1).intendedSlideOnset = _slides.at(i).intendedSlideOnset + _slides.at(i).intendedSlideDuration;
						_slides.at(i + 1).intendedOnsetFrameNumber = _slides.at(i).intendedOnsetFrameNumber + _slides.at(i).intendedFrameCount;
					}
				}

				//If there was a previous slide, mark it as finished.
				if (_currentSlide > 0) {
					CX_Slide_t &lastSlide = _slides.at(_currentSlide - 1);
					lastSlide.slideStatus = CX_Slide_t::FINISHED;

					//Now that the slide is finished, figure out its duration.
					lastSlide.actualSlideDuration = _slides.at(_currentSlide).actualSlideOnset - lastSlide.actualSlideOnset;
					lastSlide.actualFrameCount = _slides.at(_currentSlide).actualOnsetFrameNumber - lastSlide.actualOnsetFrameNumber;
				}

				//If this is the last slide, then we are done presenting slides.
				if (_currentSlide == (_slides.size() - 1)) {
					_presentingSlides = false;

					//The duration of the final frame is undefined. These could be 0.
					_slides.back().actualSlideDuration = std::numeric_limits<CX_Micros_t>::max();
					_slides.back().actualFrameCount = std::numeric_limits<uint32_t>::max();
				}

			}

			//Is there is a slide after the current one?
			if ((_currentSlide + 1) < _slides.size()) {

				//Is that slide ready to swap in?
				if ( _slides.at(_currentSlide + 1).intendedOnsetFrameNumber <= (currentFrameNumber + 1)) {
					_currentSlide++;
					_renderCurrentSlide();
				}
			}
		}
	} else if (_synchronizing) {
		if (_display->hasSwappedSinceLastCheck()) {
			_currentSlide = 0;
			_renderCurrentSlide();
			_synchronizing = false;
			_presentingSlides = true;
		}
	}

	_waitSyncCheck();
}
*/


void CX_SlidePresenter::update(void) {

	if (_presentingSlides) {

		if (_display->hasSwappedSinceLastCheck()) {

			uint64_t currentFrameNumber = _display->getFrameNumber();

			//Was the current frame just swapped in? If so, store information about the swap time.
			if (_slides.at(_currentSlide).slideStatus == CX_Slide_t::SWAP_PENDING) {

				CX_Micros_t currentSlideOnset = _display->getLastSwapTime();

				_slides.at(_currentSlide).slideStatus = CX_Slide_t::IN_PROGRESS;
				_slides.at(_currentSlide).actualOnsetFrameNumber = currentFrameNumber;
				_slides.at(_currentSlide).actualSlideOnset = currentSlideOnset;

				if (_currentSlide == 0) {
					_slides.at(0).intendedOnsetFrameNumber = currentFrameNumber;
					_slides.at(0).intendedSlideOnset = currentSlideOnset; //This is sort of weird, but true.
				}

				if (_currentSlide > 0) {
					_finishPreviousSlide();
				}

				if (_currentSlide == (_slides.size() - 1)) {
					_handleFinalSlide();
				}

				//If there is a slide after the current one, prepare it. This MUST come after _handleFinalSlide(), 
				//because if new slides are added, this has to happen for them.
				if ((_currentSlide + 1) < _slides.size()) {
					_prepareNextSlide();
				}
			}

			//Is there is a slide after the current one?
			if ((_currentSlide + 1) < _slides.size()) {
				if (_slides.at(_currentSlide + 1).intendedOnsetFrameNumber <= (currentFrameNumber + 1)) {
					_currentSlide++; //This must happen before the next slide is rendered.
					_renderCurrentSlide();
				}
			}
		}
	} else if (_synchronizing) {
		if (_display->hasSwappedSinceLastCheck()) {
			_currentSlide = 0;
			_renderCurrentSlide();
			_synchronizing = false;
			_presentingSlides = true;
		}
	}

	_waitSyncCheck();
}

void CX_SlidePresenter::_finishPreviousSlide(void) {
	CX_Slide_t &previousSlide = _slides.at(_currentSlide - 1);
	previousSlide.slideStatus = CX_Slide_t::FINISHED;

	if (_deallocateFramebuffersForCompletedSlides) {
		previousSlide.framebuffer.allocate(0, 0); //"Deallocate" the framebuffer
	}

	//Now that the slide is finished, figure out its duration.
	previousSlide.actualSlideDuration = _slides.at(_currentSlide).actualSlideOnset - previousSlide.actualSlideOnset;
	previousSlide.actualFrameCount = _slides.at(_currentSlide).actualOnsetFrameNumber - previousSlide.actualOnsetFrameNumber;
}

void CX_SlidePresenter::_handleFinalSlide(void) {
	CX_UserFunctionInfo_t info;
	info.currentSlideIndex = _currentSlide;
	info.instance = this;
	info.userStatus = CX_UserFunctionInfo_t::CONTINUE_PRESENTATION;

	unsigned int previousSlideCount = _slides.size();

	if (_userFunction != nullptr) {
		_userFunction(info);
	}

	//Start from the first new slide and go to the last new slide. This is really not neccessary.
	for (unsigned int i = previousSlideCount; i < _slides.size(); i++) {
		_slides.at(i).slideStatus = CX_Slide_t::NOT_STARTED;
	}

	//If the user requests a stop or if there is no user function, stop presenting.
	if ((info.userStatus == CX_UserFunctionInfo_t::STOP_NOW) || (_userFunction == nullptr)) {
		_presentingSlides = false;

		//The duration of the current slide is set to undefined (user may keep it on screen indefinitely).
		_slides.at(_currentSlide).actualSlideDuration = std::numeric_limits<CX_Micros_t>::max();
		_slides.at(_currentSlide).actualFrameCount = std::numeric_limits<uint32_t>::max();

		//The duration of following slides (if any) are set to 0 (never presented).
		for (unsigned int i = _currentSlide + 1; i < _slides.size(); i++) {
			_slides.at(i).actualSlideDuration = 0;
			_slides.at(i).actualFrameCount = 0;
		}

		//Deallocate all slides from here on
		for (unsigned int i = _currentSlide; i < _slides.size(); i++) {
			_slides.at(i).framebuffer.allocate(0, 0);
		}
	}

}

void CX_SlidePresenter::_prepareNextSlide(void) {
	CX_Slide_t &currentSlide = _slides.at(_currentSlide);
	CX_Slide_t &nextSlide = _slides.at(_currentSlide + 1);

	if (_errorMode == CX_SP_ErrorMode::PROPAGATE_DELAYS) {
		nextSlide.intendedSlideOnset = currentSlide.actualSlideOnset + currentSlide.intendedSlideDuration;
		nextSlide.intendedOnsetFrameNumber = currentSlide.actualOnsetFrameNumber + currentSlide.intendedFrameCount;
	} else if (_errorMode == CX_SP_ErrorMode::FIX_TIMING_FROM_FIRST_SLIDE) {
		
		nextSlide.intendedSlideOnset = currentSlide.intendedSlideOnset + currentSlide.intendedSlideDuration;
		nextSlide.intendedOnsetFrameNumber = currentSlide.intendedOnsetFrameNumber + currentSlide.intendedFrameCount;

		uint64_t endFrameNumber = nextSlide.intendedOnsetFrameNumber + nextSlide.intendedFrameCount;

		if (endFrameNumber <= _display->getFrameNumber()) {
			
			if ((_currentSlide + 2) < _slides.size()) {
				//If the next slide is not the last slide, it may be skipped

				_currentSlide++;

				_finishPreviousSlide();
				nextSlide.actualSlideDuration = 0;
				nextSlide.actualFrameCount = 0;

				Log.error("CX_SlidePresenter") << "Slide skipped at index " << _currentSlide;

				_prepareNextSlide(); //Keep skipping slides

				//return;

			} else {
				//The next slide is the last slide and may not be skipped
				Log.error("CX_SlidePresenter") << "The next slide is the last slide and may not be skipped: " << _currentSlide;
			}
		}

	}
}


void CX_SlidePresenter::_waitSyncCheck (void) {
	//By forcing the fence sync to complete before the slide is marked as ready to swap, you run into the potential
	//for a late fence sync paired with an on time write to the back buffer to result in false positives for an error
	//condition. Better a false positive than a miss, I guess.
	if (_awaitingFenceSync) {
		GLenum result = glClientWaitSync(_fenceSyncObject, 0, 10); //GL_SYNC_FLUSH_COMMANDS_BIT can be second parameter. Wait for 10 ns for sync.
		if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED) {
			if (_slides.at(_currentSlide).slideStatus == CX_Slide_t::COPY_TO_BACK_BUFFER_PENDING) {
				
				_slides.at( _currentSlide ).copyToBackBufferCompleteTime = CX::Instances::Clock.getTime();
				_awaitingFenceSync = false;

				_slides.at(_currentSlide).slideStatus = CX_Slide_t::SWAP_PENDING;

				Log.verbose("CX_SlidePresenter") << "Fence sync done for slide #" << _currentSlide;
			} else {
				Log.error("CX_SlidePresenter") << "Fence sync completed when active slide was not waiting for copy to back buffer.";
				_awaitingFenceSync = false;
			}

		}
	}
}

void CX_SlidePresenter::_renderCurrentSlide (void) {
	if (_slides.at(_currentSlide).drawingFunction != NULL) {
		_display->beginDrawingToBackBuffer();
		_slides.at(_currentSlide).drawingFunction();
		_display->endDrawingToBackBuffer();
	} else {
		_display->drawFboToBackBuffer( _slides.at(_currentSlide).framebuffer );
	}
	_fenceSyncObject = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
	glFlush();
	_awaitingFenceSync = true;
	_slides.at(_currentSlide).slideStatus = CX_Slide_t::COPY_TO_BACK_BUFFER_PENDING;
}

void CX_SlidePresenter::clearSlides (void) {
	_slides.clear();
	_currentSlide = 0;
	_presentingSlides = false;
	_synchronizing = false;
}

void CX_SlidePresenter::startSlidePresentation(void) {
	if (_display == NULL) {
		Log.error("CX_SlidePresenter") << "Cannot start slide presentation without a valid monitor attached. Use setMonitor() to attach a monitor to the SlidePresenter";
		return;
	}

	if (_slides.size() <= 0) {
		Log.warning("CX_SlidePresenter") << "Cannot start slide presentation without any slides to present.";
		return;
	}

	if (!_display->isAutomaticallySwapping()) {
		_display->BLOCKING_setSwappingState(true); //This class requires that the monitor be swapping constantly while presenting slides.
		Log.notice("CX_SlidePresenter") << "Display was not set to automatically swap at start of presentation. It was set to swap automatically in order for the slide presentation to occur.";
	}

	if (_slides.size() > 0) {

		if (_lastFramebufferActive) {
			Log.warning("CX_SlidePresenter") << "startSlidePresentation was called before last slide was finished. Call endDrawingCurrentSlide() before starting slide presentation.";
			endDrawingCurrentSlide();
		}

		CX_Micros_t framePeriod = _display->getFramePeriod();

		for (int i = 0; i < _slides.size(); i++) {
			//This doesn't need to be done here any more, it's done as slides are added
			//_slides.at(i).intendedFrameCount = _calculateFrameCount(_slides.at(i).intendedSlideDuration);

			_slides.at(i).slideStatus = CX_Slide_t::NOT_STARTED;
		}

		_synchronizing = true;
		_presentingSlides = false;

		//Wait for any ongoing operations to complete before starting frame presentation.
		_display->BLOCKING_waitForOpenGL();

		_display->hasSwappedSinceLastCheck(); //Throw away any very recent swaps.

	}
}

unsigned int CX_SlidePresenter::_calculateFrameCount (CX_Micros_t duration) {
	double framesInDuration = (double)duration / _display->getFramePeriod();
	framesInDuration = CX::round(framesInDuration, 0, CX::CX_RoundingConfiguration::ROUND_TO_NEAREST);
	return (uint32_t)framesInDuration;
}

void CX_SlidePresenter::beginDrawingNextSlide (CX_Micros_t slideDuration, string slideName) {

	if (_lastFramebufferActive) {
		Log.verbose("CX_SlidePresenter") << "The previous frame was not finished before new frame started. Call endDrawingCurrentSlide() before starting slide presentation.";
		endDrawingCurrentSlide();
	}

	if (_display == NULL) {
		Log.error("CX_SlidePresenter") << "Cannot draw slides without a valid CX_Display attached. Call setup() before calling beginDrawingNextSlide.";
		return;
	}

	if (slideDuration == 0) {
		Log.warning("CX_SlidePresenter") << "Slide named \"" << slideName << "\" with duration 0 ignored.";
		return;
	}

	_slides.push_back( CX_Slide_t() );

	_slides.back().slideName = slideName;
	Log.verbose("CX_SlidePresenter") << "Allocating framebuffer...";
	_slides.back().framebuffer.allocate( _display->getResolution().x, _display->getResolution().y, GL_RGBA, CX::getSampleCount() );
	Log.verbose("CX_SlidePresenter") << "Finished allocating.";
	
	_slides.back().intendedSlideDuration = slideDuration;
	_slides.back().intendedFrameCount = _calculateFrameCount(slideDuration);

	Log.verbose("CX_SlidePresenter") << "Beginning to draw to framebuffer.";
	_slides.back().framebuffer.begin();
	_lastFramebufferActive = true;

}

void CX_SlidePresenter::endDrawingCurrentSlide (void) {
	_slides.back().framebuffer.end();
	_lastFramebufferActive = false;
}

void CX_SlidePresenter::appendSlide (CX_Slide_t slide) {
	if (slide.intendedSlideDuration == 0) {
		Log.warning("CX_SlidePresenter") << "Slide named \"" << slide.slideName << "\" with duration 0 ignored.";
		return;
	}
	_slides.push_back( slide );
	_slides.back().intendedFrameCount = _calculateFrameCount(slide.intendedSlideDuration);
}

void CX_SlidePresenter::appendSlideFunction (void (*drawingFunction) (void), CX_Micros_t slideDuration, string slideName) {

	if (slideDuration == 0) {
		Log.warning("CX_SlidePresenter") << "Slide named \"" << slideName << "\" with duration 0 ignored.";
		return;
	}

	if (drawingFunction == NULL) {
		Log.error("CX_SlidePresenter") << "NULL pointer to drawing function given.";
		return;
	}

	if (_lastFramebufferActive) {
		Log.verbose("CX_SlidePresenter") << "The previous frame was not finished before new frame started. Call endDrawingCurrentSlide() before starting slide presentation.";
		endDrawingCurrentSlide();
	}

	_slides.push_back( CX_Slide_t() );

	_slides.back().drawingFunction = drawingFunction;
	_slides.back().intendedSlideDuration = slideDuration;
	_slides.back().intendedFrameCount = _calculateFrameCount(slideDuration);
	_slides.back().slideName = slideName;
}

vector<CX_Slide_t> CX_SlidePresenter::getSlides (void) {
	return _slides;
}

vector<CX_Micros_t> CX_SlidePresenter::getActualPresentationDurations (void) {
	vector<CX_Micros_t> durations(_slides.size());
	for (unsigned int i = 0; i < _slides.size(); i++) {
		durations[i] = _slides[i].actualSlideDuration;
	}
	return durations;
}

vector<unsigned int> CX_SlidePresenter::getActualFrameCounts (void) {
	vector<unsigned int> frameCount(_slides.size());
	for (unsigned int i = 0; i < _slides.size(); i++) {
		frameCount[i] = _slides[i].actualFrameCount;
	}
	return frameCount;
}

int CX_SlidePresenter::checkForPresentationErrors (void) {

	int presentationErrors = 0;
	for (int i = 0; i < _slides.size(); i++) {
		CX_Slide_t &sl = _slides.at(i);

		if (sl.intendedFrameCount != sl.actualFrameCount) {
			//This error does not apply to the last slide because the duration of the last slide is undefined.
			if (i != _slides.size() - 1) {
				presentationErrors++;
			}
		}

		if (sl.copyToBackBufferCompleteTime > sl.actualSlideOnset) {
			presentationErrors++;
		}
	}

	return presentationErrors;
}

string CX_SlidePresenter::getActiveSlideName (void) {
	if (_currentSlide < _slides.size()) {
		return _slides.at(_currentSlide).slideName;
	}
	return "Active slide index out of range.";
}

CX_Slide_t& CX_SlidePresenter::getSlide (unsigned int slideIndex) {
	if (slideIndex < _slides.size()) {
		return _slides.at(slideIndex);
	}
	Log.error("CX_SlidePresenter") << "getSlide: slideIndex out of range";
	return _slides.back(); //Throws if size == 0
}