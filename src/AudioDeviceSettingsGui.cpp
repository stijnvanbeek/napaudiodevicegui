/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioDeviceSettingsGui.h"

#include <imguiservice.h>
#include <nap/logger.h>
#include <nap/core.h>

#include <imgui/imgui.h>

RTTI_BEGIN_CLASS_NO_DEFAULT_CONSTRUCTOR(nap::audio::AudioDeviceSettingsGui)
		RTTI_CONSTRUCTOR(nap::Core&)
		RTTI_PROPERTY("ShowInputs", &nap::audio::AudioDeviceSettingsGui::mShowInputs, nap::rtti::EPropertyMetaData::Default)
RTTI_END_CLASS

namespace nap
{
	namespace audio
	{
		static int getIndexOf(int value, std::vector<int>& list)
		{
			for (auto i = 0; i < list.size(); ++i)
				if (list[i] == value)
					return i;

			return -1;
		}


		AudioDeviceSettingsGui::AudioDeviceSettingsGui(Core& core)
		{
			mAudioService = core.getService<PortAudioService>();
			mGuiService = core.getService<IMGuiService>();
		}


		bool AudioDeviceSettingsGui::init(utility::ErrorState &errorState)
		{
			mDriverSelection = mAudioService->getCurrentHostApiIndex();
			pollAvailableDevices(mDrivers);

			// Close the audio stream
			auto settings = mAudioService->getDeviceSettings();
			if (mAudioService->isOpened())
			{
				if (mAudioService->isActive())
				{
					auto result = mAudioService->stop(errorState);
					assert(result == true);
				}

				auto result = mAudioService->closeStream(errorState);
				assert(result == true);
			}

			updateSupportedSampleRates(); // Call this while audio stream is stopped and closed.

			// Restart the audio stream
			if (mAudioService->openStream(settings, errorState))
				mAudioService->start(errorState);

			mBufferSizeIndex = getIndexOf(mAudioService->getCurrentBufferSize(), mBufferSizes);
			mSupportedSampleRateIndex = getIndexOf(int(mAudioService->getNodeManager().getSampleRate()), mSupportedSampleRates);

			return true;
		}


		void AudioDeviceSettingsGui::drawGui(bool showInputs, bool showOutputs)
		{
			utility::ErrorState error_state;
			bool change = false;
			auto settings = mAudioService->getDeviceSettings();

			// Combo box with all available drivers
			int comboIndex = mDriverSelection + 1;
			change = ImGui::Combo("Driver", &comboIndex, [](void* data, int index, const char** out_text)
			{
				auto* drivers = static_cast<std::vector<DriverInfo>*>(data);
				if (index == 0)
					*out_text = "No Driver";
				else
					*out_text = drivers->at(index - 1).mName.c_str();
				return true;
			}, &mDrivers, mDrivers.size() + 1);
			mDriverSelection = comboIndex - 1;

			if (mDriverSelection >= 0)
			{
				// driver has changed
				if (change)
				{
					mInputDeviceSelection = -1;
					mOutputDeviceSelection = -1;

					mSupportedSampleRates.clear();
					mSupportedSampleRateIndex = -1;

					mBufferSizeIndex = getIndexOf(mAudioService->getCurrentBufferSize(), mBufferSizes);
					if (mBufferSizeIndex < 0)
						mBufferSizeIndex = 0;
				}

				assert(mDriverSelection < mDrivers.size());
				
				// Combo box with all input devices for the chosen driver
				if(showInputs)
					change = drawInputComboBox(error_state) | change;

				// Combo box with all output devices for the chosen driver
				if (showOutputs)
					change = drawOutputComboBox(error_state) | change;

				// Combo box for sample rate and buffersize
				if (!mSupportedSampleRates.empty())
					change = ImGui::Combo("Sample Rate", &mSupportedSampleRateIndex, mSupportedSampleRateNames.data(), mSupportedSampleRates.size()) || change;
				change = ImGui::Combo("Buffer Size", &mBufferSizeIndex, mBufferSizeNames, mBufferSizes.size()) || change;
			}
			
			// A change in settings has occurred, close (& open) audio stream
			if (change)
				restartAudioDevice(error_state);
		}
		
		
		void AudioDeviceSettingsGui::drawOutputGui()
		{
			utility::ErrorState error_state;
			
			if (drawOutputComboBox(error_state))
				restartAudioDevice(error_state);
		}

		
		void AudioDeviceSettingsGui::drawInputGui()
		{
			utility::ErrorState error_state;
				
			if (drawInputComboBox(error_state))
				restartAudioDevice(error_state);
		}

		
		void AudioDeviceSettingsGui::drawErrors()
		{
			if (!mAudioService->isOpened())
				ImGui::TextColored(mGuiService->getPalette().mHighlightColor4, mAudioService->getErrorMessage().c_str());
		}

		
		bool AudioDeviceSettingsGui::drawInputComboBox(utility::ErrorState& errorState)
		{
			if (mDriverSelection < 0)
			{
				ImGui::TextColored(mGuiService->getPalette().mHighlightColor4, "Audio driver not enabled.");
				return false;
			}

			bool change = false;
			
			// Update inputs
			mHasInputs = !mDrivers[mDriverSelection].mInputDevices.empty();
			
			// Combo box with all input devices for the chosen driver
			if (mHasInputs)
			{
				int comboIndex = mInputDeviceSelection + 1;
				change = ImGui::Combo("Input Device", &comboIndex, [](void* data, int index, const char** out_text)
									  {
					auto* devices = static_cast<std::vector<DeviceInfo>*>(data);
					if(index == 0)
						*out_text = "No Device";
					else
						*out_text = devices->at(index - 1).mDisplayName.c_str();
					return true;
				}, &mDrivers[mDriverSelection].mInputDevices, mDrivers[mDriverSelection].mInputDevices.size() + 1);
				mInputDeviceSelection = comboIndex - 1;
			}
			
			return change;
		}
		
		
		bool AudioDeviceSettingsGui::drawOutputComboBox(utility::ErrorState& errorState)
		{
			if (mDriverSelection < 0)
			{
				ImGui::TextColored(mGuiService->getPalette().mHighlightColor4, "Audio driver not enabled.");
				return false;
			}

			// Combo box with all output devices for the chosen driver
			int comboIndex = mOutputDeviceSelection + 1;
			bool change = ImGui::Combo("Output Device", &comboIndex, [](void* data, int index, const char** out_text)
			{
				auto* devices = static_cast<std::vector<DeviceInfo>*>(data);
				if(index == 0)
					*out_text = "No Device";
				else
					*out_text = devices->at(index - 1).mDisplayName.c_str();
				return true;
			}, &mDrivers[mDriverSelection].mOutputDevices, mDrivers[mDriverSelection].mOutputDevices.size() + 1);
			mOutputDeviceSelection = comboIndex - 1;

			return change;
		}

		
		void AudioDeviceSettingsGui::restartAudioDevice(utility::ErrorState& errorState)
		{
			if (mDriverSelection < 0 && mAudioService->isOpened() && mAudioService->isActive())
			{
				mAudioService->stop(errorState);
				return;
			}
			
			auto settings = mAudioService->getDeviceSettings();

			// apply settings
			DeviceInfo* inputDevice = nullptr;
			DeviceInfo* outputDevice = nullptr;
			DriverInfo& driver = mDrivers[mDriverSelection];
			if (mInputDeviceSelection >= 0)
				inputDevice = &driver.mInputDevices[mInputDeviceSelection];
			if (mOutputDeviceSelection >= 0)
				outputDevice = &driver.mOutputDevices[mOutputDeviceSelection];

			settings.mHostApi = mDrivers[mDriverSelection].mName;
			settings.mOutputDevice = outputDevice != nullptr ? outputDevice->mName : "";
			settings.mInputDevice = inputDevice != nullptr ? inputDevice->mName : "";
			settings.mBufferSize = mBufferSizes[mBufferSizeIndex];
			settings.mInternalBufferSize = mBufferSizes[mBufferSizeIndex];
			if (mSupportedSampleRateIndex < 0)
				mSupportedSampleRateIndex = 0;
			if (!mSupportedSampleRates.empty())
			{
				if (mSupportedSampleRateIndex >= mSupportedSampleRates.size())
					mSupportedSampleRateIndex = 0;
				settings.mSampleRate = mSupportedSampleRates[mSupportedSampleRateIndex];
			}
			else
				settings.mSampleRate = mSampleRates[0]; // We assume here that 44.1kHz is almost always supported as a default

			settings.mHostApi = mDriverSelection >= 0 ? mDrivers[mDriverSelection].mName : "";
			settings.mOutputChannelCount = outputDevice != nullptr ? outputDevice->mChannelCount: 0;
			settings.mInputChannelCount = inputDevice != nullptr ? inputDevice->mChannelCount : 0;
			
			if (mAudioService->isOpened())
			{
				if (mAudioService->isActive())
				{
					auto result = mAudioService->stop(errorState);
					assert(result == true);
				}
				
				auto result = mAudioService->closeStream(errorState);
				assert(result == true);
			}

			updateSupportedSampleRates(); // Call this while audio stream is stopped and closed.
			if (mSupportedSampleRates.empty())
				return;
			if (!isSampleRateSupported(settings.mSampleRate))
			{
				mSupportedSampleRateIndex = 0;
				settings.mSampleRate = mSupportedSampleRates[mSupportedSampleRateIndex];
			}

			if (mAudioService->openStream(settings, errorState))
			{
				mAudioService->start(errorState);
			}
		}


		void AudioDeviceSettingsGui::updateSupportedSampleRates()
		{
			mSupportedSampleRates.clear();
			mSupportedSampleRateNames.clear();

			if (mDriverSelection < 0)
				return;

			int inputDeviceIndex = -1;
			int outputDeviceIndex = -1;
			int inputChannelCount = 0;
			int outputChannelCount = 0;

			if (mInputDeviceSelection >= 0)
			{
				auto inputDevice = &mDrivers[mDriverSelection].mInputDevices[mInputDeviceSelection];
				inputDeviceIndex = inputDevice->mIndex;
				inputChannelCount = inputDevice->mChannelCount;
			}

			if (mOutputDeviceSelection >= 0)
			{
				auto outputDevice = &mDrivers[mDriverSelection].mOutputDevices[mOutputDeviceSelection];
				outputDeviceIndex = outputDevice->mIndex;
				outputChannelCount = outputDevice->mChannelCount;
			}

			assert(mSampleRates.size() == mSampleRateNames.size());
			for (int i = 0; i < mSampleRates.size(); i++)
			{
				if (mAudioService->isFormatSupported(inputDeviceIndex, outputDeviceIndex, inputChannelCount, outputChannelCount, mSampleRates[i]))
				{
					mSupportedSampleRates.push_back(mSampleRates[i]);
					mSupportedSampleRateNames.push_back(mSampleRateNames[i]);
				}
			}
		}


		bool AudioDeviceSettingsGui::isSampleRateSupported(int sampleRate) const
		{
			auto it = std::find(mSupportedSampleRates.begin(), mSupportedSampleRates.end(), sampleRate);
			return it != mSupportedSampleRates.end();
		}


		void AudioDeviceSettingsGui::pollAvailableDevices(DriverList &drivers)
		{
			mDriverSelection = -1;
			mInputDeviceSelection = -1;
			mOutputDeviceSelection = -1;

			for (auto hostApiIndex = 0; hostApiIndex < mAudioService->getHostApiCount(); ++hostApiIndex)
			{
				if (hostApiIndex == mAudioService->getCurrentHostApiIndex())
					mDriverSelection = hostApiIndex;

				DriverInfo driverInfo;
				driverInfo.mIndex = hostApiIndex;
				driverInfo.mName = mAudioService->getHostApiName(hostApiIndex);
				auto devices = mAudioService->getDevices(hostApiIndex);
				for (auto deviceIndex = 0; deviceIndex < devices.size(); deviceIndex++)
				{
					auto device = devices[deviceIndex];
					if (mHasInputs)
					{
						DeviceInfo deviceInfo;
						deviceInfo.mIndex = mAudioService->getDeviceIndex(hostApiIndex, deviceIndex);
						if (device->maxInputChannels > 0)
						{
							if (deviceInfo.mIndex == mAudioService->getCurrentInputDeviceIndex())
								mInputDeviceSelection = driverInfo.mInputDevices.size();

							deviceInfo.mChannelCount = device->maxInputChannels;
							deviceInfo.mName = device->name;
							deviceInfo.mDisplayName = deviceInfo.mName + " (" + std::to_string(deviceInfo.mChannelCount) + " inputs)";
							driverInfo.mInputDevices.push_back(deviceInfo);
						}
					}

					if (device->maxOutputChannels > 0)
					{
						DeviceInfo deviceInfo;
						deviceInfo.mIndex = mAudioService->getDeviceIndex(hostApiIndex, deviceIndex);
						if (deviceInfo.mIndex == mAudioService->getCurrentOutputDeviceIndex())
							mOutputDeviceSelection = driverInfo.mOutputDevices.size();

						deviceInfo.mChannelCount = device->maxOutputChannels;
						deviceInfo.mName = device->name;
						deviceInfo.mDisplayName = deviceInfo.mName + " (" + std::to_string(deviceInfo.mChannelCount) + " outputs)";
						driverInfo.mOutputDevices.push_back(deviceInfo);
					}
				}
				drivers.push_back(driverInfo);
			}
		}


		void AudioDeviceSettingsGui::draw()
		{
			drawGui(mShowInputs);
			drawErrors();
		}

	}
}
