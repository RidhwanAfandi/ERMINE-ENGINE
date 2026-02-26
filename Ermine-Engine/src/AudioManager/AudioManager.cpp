/* Start Header ************************************************************************/
/*!
\file       AudioManager.h
\author     Hurng Kai Rui, h.kairui, 2301278, h.kairui\@digipen.edu
\date       15/9/2025
\brief      AudioManager is where the FMOD audio engine is wrapped.
			Provides a simple interface for loading, playing, and managing sounds.

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/

#include "PreCompile.h"
#include "AudioManager.h"

using namespace Ermine;

Implementation::Implementation() {
	mpStudioSystem = NULL;
	CAudioEngine::ErrorCheck(FMOD::Studio::System::create(&mpStudioSystem));
	CAudioEngine::ErrorCheck(mpStudioSystem->initialize(32, FMOD_STUDIO_INIT_LIVEUPDATE, FMOD_INIT_PROFILE_ENABLE, NULL));

	mpSystem = NULL;
	CAudioEngine::ErrorCheck(mpStudioSystem->getCoreSystem(&mpSystem));
	// Increase DSP buffer size to reduce underruns/crackling on streaming audio.
	CAudioEngine::ErrorCheck(mpSystem->setDSPBufferSize(2048, 4));

	mnNextChannelId = 0;
}

Implementation::~Implementation() {
	CAudioEngine::ErrorCheck(mpStudioSystem->unloadAll());
	CAudioEngine::ErrorCheck(mpStudioSystem->release());
}

void Implementation::Update() {
	vector<ChannelMap::iterator> pStoppedChannels;
	for (auto it = mChannels.begin(), itEnd = mChannels.end(); it != itEnd; ++it)
	{
		bool bIsPlaying = false;
		it->second->isPlaying(&bIsPlaying);
		if (!bIsPlaying)
		{
			pStoppedChannels.push_back(it);
		}
	}
	for (auto& it : pStoppedChannels)
	{
		mChannels.erase(it);
	}
	CAudioEngine::ErrorCheck(mpStudioSystem->update());
}

Implementation* sgpImplementation = nullptr;

void CAudioEngine::Init() {
	sgpImplementation = new Implementation;
}

void CAudioEngine::Update() {
	sgpImplementation->Update();
}

void CAudioEngine::LoadSound(const std::string& strSoundName, bool b3d, bool bLooping, bool bStream)
{
	auto tFoundIt = sgpImplementation->mSounds.find(strSoundName);
	if (tFoundIt != sgpImplementation->mSounds.end())
		return;
	FMOD_MODE eMode = FMOD_DEFAULT;
	eMode |= b3d ? FMOD_3D : FMOD_2D;
	eMode |= bLooping ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
	eMode |= bStream ? FMOD_CREATESTREAM : FMOD_CREATECOMPRESSEDSAMPLE;
	FMOD::Sound* pSound = nullptr;
	CAudioEngine::ErrorCheck(sgpImplementation->mpSystem->createSound(strSoundName.c_str(), eMode, nullptr, &pSound));
	if (pSound) {
		sgpImplementation->mSounds[strSoundName] = pSound;
	}
}

void CAudioEngine::UnLoadSound(const std::string& strSoundName)
{
	auto tFoundIt = sgpImplementation->mSounds.find(strSoundName);
	if (tFoundIt == sgpImplementation->mSounds.end())
		return;
	CAudioEngine::ErrorCheck(tFoundIt->second->release());
	sgpImplementation->mSounds.erase(tFoundIt);
}

// Updated to use Vector3D instead of Vector3
int CAudioEngine::PlaySounds(const string& strSoundName, const Vector3D& vPosition, float fVolumedB)
{
	// Check if sound exists, load if needed
	auto tFoundIt = sgpImplementation->mSounds.find(strSoundName);
	if (tFoundIt == sgpImplementation->mSounds.end())
	{
		LoadSound(strSoundName);
		tFoundIt = sgpImplementation->mSounds.find(strSoundName);
		if (tFoundIt == sgpImplementation->mSounds.end())
		{
			cout << "Failed to load sound: " << strSoundName << endl;
			return -1; // Return -1 for failure, not a random channel ID
		}
	}

	// Try to play the sound
	FMOD::Channel* pChannel = nullptr;
	FMOD_RESULT result = sgpImplementation->mpSystem->playSound(tFoundIt->second, nullptr, true, &pChannel);

	if (CAudioEngine::ErrorCheck(result) != 0 || !pChannel) {
		cout << "Failed to play sound: " << strSoundName << endl;
		return -1; // Return -1 for failure
	}

	// Get channel ID AFTER successful playback
	int nChannelId = sgpImplementation->mnNextChannelId++;

	// Configure the channel
	FMOD_MODE currMode;
	tFoundIt->second->getMode(&currMode);
	if (currMode & FMOD_3D) {
		FMOD_VECTOR position = VectorToFmod(vPosition);
		CAudioEngine::ErrorCheck(pChannel->set3DAttributes(&position, nullptr));
	}

	CAudioEngine::ErrorCheck(pChannel->setVolume(dbToVolume(fVolumedB)));
	CAudioEngine::ErrorCheck(pChannel->setPaused(false)); // Unpause to start playing

	// Store the channel for later control
	sgpImplementation->mChannels[nChannelId] = pChannel;

	cout << "Successfully playing sound on channel: " << nChannelId << endl;
	return nChannelId; // Return valid channel ID
}

// Updated to use Vector3D instead of Vector3
void CAudioEngine::SetChannel3dPosition(int nChannelId, const Vector3D& vPosition)
{
	auto tFoundIt = sgpImplementation->mChannels.find(nChannelId);
	if (tFoundIt == sgpImplementation->mChannels.end())
		return;

	FMOD_VECTOR position = VectorToFmod(vPosition);
	CAudioEngine::ErrorCheck(tFoundIt->second->set3DAttributes(&position, NULL));
}

void CAudioEngine::SetChannelVolume(int nChannelId, float fVolumedB)
{
	auto tFoundIt = sgpImplementation->mChannels.find(nChannelId);
	if (tFoundIt == sgpImplementation->mChannels.end())
		return;

	CAudioEngine::ErrorCheck(tFoundIt->second->setVolume(dbToVolume(fVolumedB)));
}

void CAudioEngine::SetChannelPaused(int nChannelId, bool paused)
{
	auto tFoundIt = sgpImplementation->mChannels.find(nChannelId);
	if (tFoundIt == sgpImplementation->mChannels.end())
		return;

	CAudioEngine::ErrorCheck(tFoundIt->second->setPaused(paused));
}

void CAudioEngine::LoadBank(const std::string& strBankName, FMOD_STUDIO_LOAD_BANK_FLAGS flags) {
	auto tFoundIt = sgpImplementation->mBanks.find(strBankName);
	if (tFoundIt != sgpImplementation->mBanks.end())
		return;
	FMOD::Studio::Bank* pBank;
	CAudioEngine::ErrorCheck(sgpImplementation->mpStudioSystem->loadBankFile(strBankName.c_str(), flags, &pBank));
	if (pBank) {
		sgpImplementation->mBanks[strBankName] = pBank;
	}
}

void CAudioEngine::LoadEvent(const std::string& strEventName) {
	auto tFoundit = sgpImplementation->mEvents.find(strEventName);
	if (tFoundit != sgpImplementation->mEvents.end())
		return;
	FMOD::Studio::EventDescription* pEventDescription = NULL;
	CAudioEngine::ErrorCheck(sgpImplementation->mpStudioSystem->getEvent(strEventName.c_str(), &pEventDescription));
	if (pEventDescription) {
		FMOD::Studio::EventInstance* pEventInstance = NULL;
		CAudioEngine::ErrorCheck(pEventDescription->createInstance(&pEventInstance));
		if (pEventInstance) {
			sgpImplementation->mEvents[strEventName] = pEventInstance;
		}
	}
}

void CAudioEngine::PlayEvent(const string& strEventName) {
	auto tFoundit = sgpImplementation->mEvents.find(strEventName);
	if (tFoundit == sgpImplementation->mEvents.end()) {
		LoadEvent(strEventName);
		tFoundit = sgpImplementation->mEvents.find(strEventName);
		if (tFoundit == sgpImplementation->mEvents.end())
			return;
	}
	tFoundit->second->start();
}

void CAudioEngine::StopEvent(const string& strEventName, bool bImmediate) {
	auto tFoundIt = sgpImplementation->mEvents.find(strEventName);
	if (tFoundIt == sgpImplementation->mEvents.end())
		return;
	FMOD_STUDIO_STOP_MODE eMode;
	eMode = bImmediate ? FMOD_STUDIO_STOP_IMMEDIATE : FMOD_STUDIO_STOP_ALLOWFADEOUT;
	CAudioEngine::ErrorCheck(tFoundIt->second->stop(eMode));
}

bool CAudioEngine::IsEventPlaying(const string& strEventName) {
	auto tFoundIt = sgpImplementation->mEvents.find(strEventName);
	if (tFoundIt == sgpImplementation->mEvents.end())
		return false;

	FMOD_STUDIO_PLAYBACK_STATE state;
	FMOD_RESULT result = tFoundIt->second->getPlaybackState(&state);

	if (ErrorCheck(result) != 0) {
		return false; // Error occurred
	}

	return (state == FMOD_STUDIO_PLAYBACK_PLAYING);
}

void CAudioEngine::GetEventParameter(const string& strEventName, const string& strParameterName, float* parameter) {
	auto tFoundIt = sgpImplementation->mEvents.find(strEventName);
	if (tFoundIt == sgpImplementation->mEvents.end())
		return;

	// Use the newer parameter API
	CAudioEngine::ErrorCheck(tFoundIt->second->getParameterByName(strParameterName.c_str(), parameter));
}

void CAudioEngine::SetEventParameter(const string& strEventName, const string& strParameterName, float fValue) {
	auto tFoundIt = sgpImplementation->mEvents.find(strEventName);
	if (tFoundIt == sgpImplementation->mEvents.end())
		return;

	// Use the newer parameter API
	CAudioEngine::ErrorCheck(tFoundIt->second->setParameterByName(strParameterName.c_str(), fValue));
}

// Updated to use Vector3D instead of Vector3
FMOD_VECTOR CAudioEngine::VectorToFmod(const Vector3D& vPosition) {
	FMOD_VECTOR fVec;
	fVec.x = vPosition.x;
	fVec.y = vPosition.y;
	fVec.z = vPosition.z;
	return fVec;
}

int CAudioEngine::ErrorCheck(FMOD_RESULT result) {
	if (result != FMOD_OK) {
		cout << "FMOD ERROR " << result << endl;
		return 1;
	}
	// cout << "FMOD all good" << endl;
	return 0;
}

float  CAudioEngine::dbToVolume(float dB)
{
	return powf(10.0f, 0.05f * dB);
}

float  CAudioEngine::VolumeTodB(float volume)
{
	return 20.0f * log10f(volume);
}

bool CAudioEngine::IsPlaying(int nChannelId) {
	auto tFoundIt = sgpImplementation->mChannels.find(nChannelId);
	if (tFoundIt == sgpImplementation->mChannels.end())
		return false;

	bool isPlaying = false;
	tFoundIt->second->isPlaying(&isPlaying);
	return isPlaying;
}

void CAudioEngine::StopChannel(int nChannelId) {
	auto tFoundIt = sgpImplementation->mChannels.find(nChannelId);
	if (tFoundIt == sgpImplementation->mChannels.end())
		return;

	CAudioEngine::ErrorCheck(tFoundIt->second->stop());
	sgpImplementation->mChannels.erase(tFoundIt);
}

void CAudioEngine::StopAllChannels() {
	for (auto& channel : sgpImplementation->mChannels) {
		CAudioEngine::ErrorCheck(channel.second->stop());
	}
	sgpImplementation->mChannels.clear();
}

void CAudioEngine::PauseAllChannels() {
	for (auto& channel : sgpImplementation->mChannels) {
		CAudioEngine::ErrorCheck(channel.second->setPaused(true));
	}
	//std::cout << "Paused " << sgpImplementation->mChannels.size() << " channels" << std::endl;
}

void CAudioEngine::ResumeAllChannels() {
	for (auto& channel : sgpImplementation->mChannels) {
		CAudioEngine::ErrorCheck(channel.second->setPaused(false));
	}
	//std::cout << "Resumed " << sgpImplementation->mChannels.size() << " channels" << std::endl;
}


void CAudioEngine::SetListenerPosition(const Vector3D& position) {
	FMOD_VECTOR pos = VectorToFmod(position);
	FMOD_VECTOR vel = { 0, 0, 0 }; // No velocity by default
	FMOD_VECTOR forward = { 0, 0, 1 }; // Default forward
	FMOD_VECTOR up = { 0, 1, 0 }; // Default up

	CAudioEngine::ErrorCheck(sgpImplementation->mpSystem->set3DListenerAttributes(
		0, // listener index (usually 0 for single listener)
		&pos,
		&vel,
		&forward,
		&up
	));
}

void CAudioEngine::SetListenerOrientation(const Vector3D& forward, const Vector3D& up) {
	FMOD_VECTOR pos = { 0, 0, 0 }; // Keep current position
	FMOD_VECTOR vel = { 0, 0, 0 }; // No velocity
	FMOD_VECTOR fwd = VectorToFmod(forward);
	FMOD_VECTOR upVec = VectorToFmod(up);

	CAudioEngine::ErrorCheck(sgpImplementation->mpSystem->set3DListenerAttributes(
		0, // listener index
		nullptr, // Don't change position (pass nullptr)
		&vel,
		&fwd,
		&upVec
	));
}

void CAudioEngine::SetListenerAttributes(const Vector3D& position,
	const Vector3D& velocity,
	const Vector3D& forward,
	const Vector3D& up) {
	FMOD_VECTOR pos = VectorToFmod(position);
	FMOD_VECTOR vel = VectorToFmod(velocity);
	FMOD_VECTOR fwd = VectorToFmod(forward);
	FMOD_VECTOR upVec = VectorToFmod(up);

	CAudioEngine::ErrorCheck(sgpImplementation->mpSystem->set3DListenerAttributes(
		0, // listener index
		&pos,
		&vel,
		&fwd,
		&upVec
	));
}

FMOD::System* CAudioEngine::GetCoreSystem() {
	return sgpImplementation ? sgpImplementation->mpSystem : nullptr;
}

void CAudioEngine::Shutdown() {
	delete sgpImplementation;
}
