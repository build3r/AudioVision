#include "pch.h"
#include "AudioPlayer.h"
#include <iostream>
#include <fstream>
#include <cstdint>


AudioPlayer::AudioPlayer()
{
	//open default audio device
	device = alcOpenDevice(NULL);

	ALCint attrs[] = { ALC_HRTF_ID_SOFT, ALC_TRUE,
	0 };

	if (device) {
		context = alcCreateContext(device, attrs);
		if (!alcMakeContextCurrent(context)) {
			std::cout << "failed to make default context\n";
		}
	}

	assert(device, alcGetIntegerv(ALC_HRTF_STATUS_SOFT) == ALC_HRTF_ENABLED_SOFT, "HRTF not enabled");

	//reset error state
	alGetError();

	alGenBuffers(numBuffers, bufferNames);
	ALenum error_code = alGetError();
	if (error_code != AL_NO_ERROR) {
		std::cout << "AudioPlayer was unable to generate buffers";
	}

	loadWave("resources/soundfiles/space_oddity.wav", bufferNames[0]);

	error_code = alGetError();
	alGenSources(maxNumSoundSources, sourceNames);

	error_code = alGetError();

	if (error_code == AL_OUT_OF_MEMORY) {
		std::cout << "Audioplayer ran out of memory while generating sounds";
	}

	if (error_code == AL_INVALID_VALUE) {
		std::cout << "There are not enough non-memory resources to create all the requested sources, or the array pointer is not valid";
	}

	if (error_code == AL_INVALID_OPERATION) {
		std::cout << "There is no context to create sources in.";
	}

	if (error_code != AL_NO_ERROR) {
		std::cout << "AudioPlayer was unable to generate sound sources";
	}

	ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };

	/* set orientation */
	alListener3f(AL_POSITION, 0, 0, 0);
	alListener3f(AL_VELOCITY, 0, 0, 0);
	alListenerfv(AL_ORIENTATION, listenerOri);

	alSourcei(sourceNames[0], AL_BUFFER, bufferNames[0]);
	alSourcei(sourceNames[0], AL_LOOPING, AL_TRUE);
	alSourcePlay(sourceNames[0]);

	float angle = 0;


	while (1) {
		
		angle += .001;
		std::cout << angle << "\n";
		if (angle > 3.14159 * 2) {
			angle -= 3.14159 * 2;
		}

		float z = 0;
		float x = 10 * cos(angle);
		float y = 10 * sin(angle);
		alSource3f(sourceNames[0], AL_POSITION, x, y, z);
	}

	system("pause");

}

void AudioPlayer::loadWave(const char* filename, ALuint buffer) {
	std::ifstream inputStream;

	inputStream.open(filename, std::fstream::binary);
	//http://soundfile.sapp.org/doc/WaveFormat/ grabbing information from places as described here
	
	if (!inputStream) {
		std::cout << "file not opened";
	}

	wave_preamble_t wavePreamble;
	inputStream.read((char*) &wavePreamble, sizeof(wave_preamble_t));
	
	ALenum format = getOpenAlSoundFormat(wavePreamble.numChannels, wavePreamble.bitsPerSample);

	ALvoid* pcmData = malloc(wavePreamble.dataChunkSize);
	inputStream.seekg(44);
	inputStream.read((char *)pcmData, wavePreamble.dataChunkSize);

	alBufferData(buffer, format, pcmData, wavePreamble.dataChunkSize, wavePreamble.sampleRate);

	ALenum error_code = alGetError();

	if (error_code == AL_OUT_OF_MEMORY) {
		std::cout << "There is not enough memory available to create this buffer.";
	}

	if (error_code == AL_INVALID_VALUE) {
		std::cout << "The size parameter is not valid for the format specified, the buffer is in use, or the data is a NULL pointer.";
	}
	
	if (error_code == AL_INVALID_ENUM) {
		std::cout << "The specified format does not exist.";
	}

	pcms.push_back(pcmData);
}

ALenum AudioPlayer::getOpenAlSoundFormat(uint16_t numChannels, uint8_t bitsPerSample)
{
	if (numChannels > 1) {
		//stereo
		if (bitsPerSample == 8) {
			return AL_FORMAT_STEREO8;
		}
		if (bitsPerSample == 16) {
			return AL_FORMAT_STEREO16;
		}
	}
	else if (numChannels == 1) {
		//mono
		if (bitsPerSample == 8) {
			return AL_FORMAT_MONO8;
		}
		if (bitsPerSample == 16) {
			return AL_FORMAT_MONO16;
		}
	}

	throw "Invalid Sound Format";
}

AudioPlayer::~AudioPlayer()
{
	alcCloseDevice(device);
	alDeleteBuffers(numBuffers, bufferNames);
	for (ALvoid* pcmData : pcms) {
		free(pcmData);
	}
}
