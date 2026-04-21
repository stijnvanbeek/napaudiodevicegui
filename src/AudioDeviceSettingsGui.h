/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#pragma once

#include <map>

#include <Gui/Gui.h>
#include <audio/service/portaudioservice.h>

namespace nap
{
    namespace audio
    {

        /**
         * Object that draws a gui to edit settings for the AudioService at runtime.
         * Enables selecting of audio devices for input and output and changing the buffer size and sample rate.
         */
        class NAPAPI AudioDeviceSettingsGui : public gui::Gui
        {
            RTTI_ENABLE(gui::Gui)
        public:
            bool mShowInputs = true; ///< Property: 'ShowInputs' Indicates whether to show and control the input device as well

            /**
             * Constructor
             * @param Core reference to core to acquire service references.
             */
            AudioDeviceSettingsGui(Core& core);

            // Inherited from Gui
            bool init(utility::ErrorState& errorState) override;
			
            /**
             * Draws the Gui for the audio service configuration settings.
             * @param showInputs whether to show input device options
             */
            void drawGui(bool showInputs = true, bool showOutputs = true);
			
			/**
			 * Draws the Gui for input device options only.
			 */
			void drawInputGui();
			
			/**
			 * Draws the Gui for output device options only.
			 */
			void drawOutputGui();
			
			/**
			 * Draws error messages.
			 */
			void drawErrors();
			
        private:
            // Inherited from gui::Gui
            void draw() override;

        	// Cached information about an available input or output device.
            struct DeviceInfo
            {
                int mIndex = -1;				// Index local to the host API
                std::string mName = "";			// Name of the device as used by port audio for identification
				std::string mDisplayName = "";	// Display name of the device in the GUI
                int mChannelCount = 0;			// Number of available audio channels
            };

        	// Cached information about an available driver (aka host API)
            struct DriverInfo
            {
            	int mIndex = -1;						// Index of the host API in portaudio
                std::string mName = "";					// Name of the host API used by portaudio
                std::vector<DeviceInfo> mInputDevices;	// List of available input devices
                std::vector<DeviceInfo> mOutputDevices;	// Lis of available output devices
            };

        	using DriverList = std::vector<DriverInfo>;	// List of available drivers (host APIs) with their supported devices.

        private:
			/**
			 * Draws the input combo box. Returns whether it has changed.
			 */
			bool drawInputComboBox(utility::ErrorState& errorState);
			
			/**
			 * Draws the output combo box. Returns whether it has changed.
			 */
			bool drawOutputComboBox(utility::ErrorState& errorState);
			
			/**
			 * Restarts audio device. Called by draw functions after a settings change.
			 */
			void restartAudioDevice(utility::ErrorState& errorState);

        	// Polls which samplerates are supported with the currently selected driver and input and output devices.
        	void updateSupportedSampleRates();

        	// Checks if the given samplerate is among the samplerates resulting from te last call to updateSupportedSampleRates()
        	bool isSampleRateSupported(int sampleRate) const;

        	// Reinitializes mDrivers list
        	// Also updates mDriverSelection, mInputDeviceSelection and mOutputDeviceSelection to point to current devices used by portaudio.
        	void pollAvailableDevices(DriverList& drivers);
			
	        PortAudioService* mAudioService = nullptr;
            IMGuiService* mGuiService = nullptr;

            int mDriverSelection = 0;			// Current driver selection, can be -1 if no driver is selected
            int mInputDeviceSelection = 0;		// Current input device selection (local to driver). Can be -1 if no input device selected.
            int mOutputDeviceSelection = 0;		// Current output device selection (local to driver). Can be -1 if no output device selected.
            int mBufferSizeIndex = 0;			// Currently selected buffersize as index into mBufferSizes
            int mSupportedSampleRateIndex = 0;	// Currently selected samplerate as index into mSupportedSampleRate
#ifdef WIN32
            // buffersize 2048 is currently only supported on windows
            std::vector<int> mBufferSizes = { 64, 128, 256, 512, 1024, 2048 };
            const char* const mBufferSizeNames[6] = { "64", "128", "256", "512", "1024", "2048" };
#else
            std::vector<int> mBufferSizes = { 64, 128, 256, 512, 1024 };
            const char* const mBufferSizeNames[5] = { "64", "128", "256", "512", "1024" };
#endif
            std::vector<int> mSampleRates = { 44100, 48000, 88200, 96000, 192000 };
            std::array<const char*, 5> mSampleRateNames = { "44100", "48000", "88200", "96000", "192000" };
        	std::vector<int> mSupportedSampleRates;
        	std::vector<const char*> mSupportedSampleRateNames;

            DriverList mDrivers;

            bool mHasInputs = true;
        };

    }

}
