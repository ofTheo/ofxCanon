#ifndef OFXCANONCOMMANDOPENSESSIONH
#define OFXCANONCOMMANDOPENSESSIONH

#include "ofxCanonCommand.h"
#include "ofxCanonDebug.h"
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include "ofMain.h"
#include "ofxLog.h"

class ofxCanonCommandOpenSession : public ofxCanonCommand {
public:
	ofxCanonCommandOpenSession(std::string sName, ofxCanonModel* pModel)
		:ofxCanonCommand(sName,pModel)
	{
	}

	virtual bool execute() {
		OFXLOG("ofxCanon: (command) execute open session in threadd: " << boost::this_thread::get_id());
		EdsError err = EDS_ERR_OK;
		err = EdsOpenSession(model->getCamera());
		bool locked = false;

		if(err == EDS_ERR_OK) {
			// Preservation ahead is set to PC
			EdsUInt32 save_to = kEdsSaveTo_Host;
			err = EdsSetPropertyData(
				 model->getCamera()
				,kEdsPropID_SaveTo
				,0
				,sizeof(save_to)
				,&save_to
			);
		}

		// Lock UI
		if(err == EDS_ERR_OK) {
			err = EdsSendStatusCommand(
				 model->getCamera()
				,kEdsCameraStatusCommand_UILock
				,0
			);
		}

		if (EDS_ERR_OK)
			locked = true;

		if (err == EDS_ERR_OK) {
			EdsCapacity capacity = {0x7FFFFFFF, 0x1000, 1};
			err = EdsSetCapacity(model->getCamera(), capacity);
		}


		// Unlock UI
		if (locked) {
			err = EdsSendStatusCommand(
				 model->getCamera()
				,kEdsCameraStatusCommand_UIUnLock
				,0
			);
		}

		// Show error:
		if(err != EDS_ERR_OK) {
			OFXLOG("ERROR: ofxCanonCommandOpenSession(): " << ofxCanonErrorToString(err) << "");
			//boost::shared_ptr<ofxObservableEvent> e(new ofxObservableEvent("internal_error"));
			ofxObservableEvent e("internal_error");
			model->notifyObservers(e);
			return false;
			//model->setSessionOpen(false);
		}
		else {
			model->setSessionOpen(true);
		}
        //boost::shared_ptr<ofxObservableEvent> e(new ofxObservableEvent("opened_session"));
        ofxObservableEvent e("opened_session");
		model->notifyObservers(e);
		//ofxObservableEvent e("opened_session");
		//model->notifyObservers(&e);
		return true;
	}
};
#endif
