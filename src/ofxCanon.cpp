#include "ofxCanon.h"

ofxCanon::ofxCanon()
	:initialized(false)
	,controller(NULL)
	,evf_started(false)
	,is_initializing(false)
	,model(NULL)
{
	model = new ofxCanonModel();
	controller = new ofxCanonController();
	controller->init(this, model);
}

ofxCanon::~ofxCanon() {
    OFXLOG("~~~~ ofxCanon");
	shutdown();
}

void ofxCanon::shutdown() {
    OFXLOG("ofxCanon::shutdown");
	if(evf_started) {
	    OFXLOG("ofxCanon::shutdown - end evf");
		endEvf();
	}
	if(model->isSessionOpen()) {
	    OFXLOG("ofxCanon::shutdown - close session");
		closeSession();
	}
	if(controller != NULL) {
	    OFXLOG("ofxCanon::shutdown - shutdown controller");
		controller->shutdown();
	}
	if(is_sdk_loaded) {
	    OFXLOG("ofxCanon::shutdown - terminate SDK");
		EdsTerminateSDK();
	}
	// @todo maybe also release the camera reference from init()?
}

bool ofxCanon::isInitializing() {
	return is_initializing;
}

void ofxCanon::resetInit() {
	is_initializing = false;
	initialized = false;
}

bool ofxCanon::init(int nCameraID, string sDownloadDir) {
	if(isInitializing())
		return false;
	EdsError err = EDS_ERR_OK;
	EdsCameraListRef camera_list = NULL;
	EdsCameraRef camera = NULL;
	EdsUInt32 count = 0;
	is_initializing = true;
	cout << "ofxCanon: start initializing the SDK" << std::endl;

	if(!is_sdk_loaded)
		err = EdsInitializeSDK();
	cout << "ofxCanon: initialized..." <<std::endl;
	if(err == EDS_ERR_OK) {
		is_sdk_loaded = true;
		cout << "ofxCanon: sdk initialized" << std::endl;
	}
	else {
		cout << "ofxCanon: could not initialize the Canon SDK" << std::endl;
		resetInit();
	}

	if(err == EDS_ERR_OK) {
		err = EdsGetCameraList(&camera_list);
	}

	if(err == EDS_ERR_OK) {
		cout << "ofxCanon: camera list loaded" << std::endl;

		err = EdsGetChildCount(camera_list, &count);
		if (count == 0) {
			err = EDS_ERR_DEVICE_NOT_FOUND;
			cout << "ofxCanon: no camera's found." << std::endl;
			resetInit();
			return false;
		}
	}

	if(err == EDS_ERR_OK) {
		err = EdsGetChildAtIndex(camera_list, nCameraID, &camera);
		cout << "ofxCanon: child count retrieved" << std::endl;
	}


	EdsDeviceInfo device_info;
	if(err == EDS_ERR_OK) {
		cout << "ofxCanon: camera at index: " << nCameraID << " retrieved" << std::endl;
		err = EdsGetDeviceInfo(camera, &device_info);
		if(err == EDS_ERR_OK && camera == NULL)
			err = EDS_ERR_DEVICE_NOT_FOUND;
	}

	if(camera_list != NULL)
		EdsRelease(camera_list);

	if(err != EDS_ERR_OK) {
		cout << "ofxCanon: ERROR: Cannot detect camera!" << std::endl;
		resetInit();
		return false;
	}

	if (err == EDS_ERR_OK) {
		cout << "ofxCanon: Camera protocol version: " << device_info.deviceSubType << std::endl;
		cout << "ofxCanon: Camera protocol description " << device_info.szDeviceDescription << std::endl;
		cout << "ofxCanon: Camera protocol portname " << device_info.szPortName << std::endl;
		cout << "ofxCanon: Initialized correctly using camera ID: " << nCameraID << std::endl;


		if(model != NULL)
			model->setCamera(camera);

		// Set property event handler.
		if (err == EDS_ERR_OK) {
			err = EdsSetPropertyEventHandler(
				camera
				,kEdsPropertyEvent_All
				,ofxCanonCameraEventListener::handlePropertyEvent
				,(EdsVoid *)controller
			);
		}

		// Set object event handler.
		if(err == EDS_ERR_OK) {
			err = EdsSetObjectEventHandler(
				 camera
				,kEdsObjectEvent_All
				,ofxCanonCameraEventListener::handleObjectEvent
				,(EdsVoid *)controller
			);
		}
		else{
			std::cout << "ofxCanon: ERROR: Cannot add property event listener: " << ofxCanonErrorToString(err) << std::endl;
		}

		// Set state event handler.
		if(err == EDS_ERR_OK) {
			err = EdsSetCameraStateEventHandler(
				camera
				,kEdsStateEvent_All
				,ofxCanonCameraEventListener::handleStateEvent
				,(EdsVoid *)controller
			);
		}
		else {
			std::cout << "ofxCanon: ERROR: Cannot add object event listener: " << ofxCanonErrorToString(err) << std::endl;
		}

		if(err != EDS_ERR_OK) {
			std::cout << "ofxCanon: ERROR: Cannot add state event listener: " << ofxCanonErrorToString(err) << std::endl;
		}

		initialized = true;
		is_initializing = false;

		// did we started earlier alread?
		if(!controller->isRunning()) {
			setupActionSources();
			setupObserver();
			controller->run();
		}
		else {
			// we started earlier.. just open the session again.
			openSession();
		}
	}
	std::cout << "ofxCanon: init() ready" << std::endl;
	return true;
}


void ofxCanon::update() {
	if(controller != NULL) {
		controller->update();
	}
}

bool ofxCanon::openSession() {
	performAction("open_session");

	return true;
}

void ofxCanon::closeSession() {
	performAction("close_session");
}

void ofxCanon::takePicture() {
	performAction("take_picture");
}

void ofxCanon::startEvf() {
	performAction("start_evf");
	evf_started = true;
}

void ofxCanon::endEvf() {
	performAction("end_evf");
	evf_started = false;
}

/**
 * setup actions.. these could be action buttons for example. Though
 * as we don't have a GUI, these actions can be execute by using appropriate
 * method.
 */
void ofxCanon::setupActionSources() {
	addActionSource("open_session");
	addActionSource("close_session");
	addActionSource("take_picture");
	addActionSource("download");
	addActionSource("start_evf");
	addActionSource("end_evf");
}

void ofxCanon::setupObserver() {

	picture_box = boost::shared_ptr<ofxCanonPictureBox>(new ofxCanonPictureBox(this));

	// Controller listens to picturebox actions
	picture_box->addActionListener(controller);
	model->addObserver(picture_box);
	// model->addObserver(shared_from_this()); // TODO shared_from_this only works when ofxCanon is owned by a shared_ptr
}

//void ofxCanon::update(ofxObservable* pFrom, ofxObservableEvent *pEvent) {
//void ofxCanon::update(boost::shared_ptr<ofxObservable> pFrom, boost::shared_ptr<ofxObservableEvent> pEvent) {
void ofxCanon::update(ofxObservable& pFrom, const ofxObservableEvent& pEvent) {
	// sometimes the camera gives an internal error, here we "restart" the
	// connections/sdk which seems to work.
	if(pEvent.getEvent() == "internal_error") {
		std::cout << "ofxCanon: handling internal_error: reset init." <<std::endl;
		closeSession();
		resetInit();
	}
}

void ofxCanon::draw(float nX, float nY, float nWidth , float nHeight) {
	// @todo use callback and set pixels when we got new pixels from
	// the EVF or when a picture has been taken.
	picture_box->draw(nX, nY, nWidth, nHeight);
}

void ofxCanon::addActionSource(std::string sCommand) {
	ofxActionSource* source = new ofxActionSource();
	source->setActionCommand(sCommand);
	std::pair<std::string, ofxActionSource*> action_source(sCommand, source);
	action_sources.insert(action_source);
	source->addActionListener(controller);
}

bool ofxCanon::performAction(std::string sCommand) {
	if(!isInitialized())
		return false;
	ofxActionSource* source = action_sources[sCommand];
	if(!source) {
		std::cout << "ofxCanon: ERROR: Could not find action: " << sCommand << std::endl;
		return false;
	}
	source->fireEvent();
	return true;
}

bool ofxCanon::isInitialized() {
	return initialized;
}

//ofxCanonPictureBox* ofxCanon::getPictureBox() {
boost::shared_ptr<ofxCanonPictureBox> ofxCanon::getPictureBox() {
	return picture_box;
}
