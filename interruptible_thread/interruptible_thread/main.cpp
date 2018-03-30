#include "interruptible_thread.h"
#include <cstdint>
#include <list>
#include "lock_list.h"

#define _USE_MATH_DEFINES

#include <math.h>

//http://www.cplusplus.com/forum/general/145283/
typedef uint8_t UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;

typedef UInt32 SourceID;

typedef float Float32;

struct WaveFormatHeader
{
	UInt8 format[4];//like WAVE OR AVI
	//TODO support this
	//http://audiocoding.ru/article/2008/05/22/wav-file-structure.html
	UInt16 audioTag;//2 bytes
	UInt16 channels;//2 bytes
	UInt32 samplesPerSec;//4 bytes
	UInt32 avgBytesPerSec;//4 bytes
	UInt16 blockAlign;//2 bytes
	UInt16 bitsPerSample;//2 bytes
};


class AudioBuffer
{
public:
	/// Constructor.
	AudioBuffer() :
		m_maxFrames(0),
		m_data(nullptr),
		m_actualFrames(0)
	{
	}

	~AudioBuffer()
	{
		clearData();
	}

	void clearData()
	{
		if (m_data)
		{
			delete[] m_data;
			m_data = nullptr;
		}
	}

	inline bool initialize(const UInt32 max_frames, const WaveFormatHeader& bufferFormat)
	{
		if (max_frames == 0)
		{
			return false;
		}

		if(bufferFormat.channels < 1)
		{
			return false;
		}

		if(bufferFormat.samplesPerSec == 0)
		{
			return false;
		}

		clearData();

		m_maxFrames = max_frames;
		m_bufferFormat = bufferFormat;

		m_data = new float[m_maxFrames * m_bufferFormat.channels];
		if (m_data)
		{
			return true;
		}

		return false;
	}

	inline bool HasData()const
	{
		return nullptr != m_data;
	}

	float* getData()
	{
		return m_data;
	}

	const WaveFormatHeader& getFormat()const
	{
		return m_bufferFormat;
	}

	bool mixData(AudioBuffer* otherBuffer)
	{
		//TODO: support conversion between different buffer formats
		if (!(HasData() && otherBuffer && otherBuffer->HasData()))
		{
			return false;
		}

		if (m_maxFrames < otherBuffer->getActualFrames())
		{
			return false;
		}

		UInt32 mixPart = (otherBuffer->getActualFrames() < m_actualFrames) ? otherBuffer->getActualFrames() : m_actualFrames;
		UInt32 i = 0;

		float* otherData = otherBuffer->getData();
		for (i = 0; i < mixPart; i++)
		{
			m_data[i] += otherData[i];
		}

		if (mixPart < otherBuffer->getActualFrames())
		{
			for (i = mixPart; i < otherBuffer->getActualFrames(); i++)
			{
				m_data[i] = otherData[i];
			}
			m_actualFrames = otherBuffer->getActualFrames();
		}
		return true;
	}

	inline UInt32 getMaxFrames() const { return m_maxFrames; }
	inline UInt32 getActualFrames() const { return m_actualFrames; }
	inline void setActualFrames(const UInt32 actualFrames) { m_actualFrames = actualFrames; }

protected:

	float*          m_data;

	WaveFormatHeader m_bufferFormat;

	UInt32 m_maxFrames;
	UInt32 m_actualFrames;
};

class AudioBlockSettings
{
public:
	virtual ~AudioBlockSettings() {};
	UInt32 bufferSize;
};

class IAudioBlock
{
public:
	virtual ~IAudioBlock() {}

	virtual bool initialize(const std::shared_ptr<AudioBlockSettings>& settings) = 0;
	virtual void finalize() = 0;
	virtual void stopLoop() = 0;
	virtual void reset() = 0;
	virtual bool isFinished() = 0;
	virtual AudioBuffer* getResult() = 0;
	virtual void update() = 0;
};

class SimpleSinBlockSettings: public AudioBlockSettings
{
public:
	double startPhase;
	double defaultGain;
	double defaultFreq;
	double time;
	UInt32 sampleRate;
};


class SimpleSinBlock : public IAudioBlock
{
public:
	SimpleSinBlock():
		m_elapsedTime(0),
		m_phase(0),
		m_gain(0),
		m_freq(0),
		m_settings(nullptr)
	{
	}

	bool initialize(const std::shared_ptr<AudioBlockSettings>& settings)override
	{
		//TODO: checks here
		m_settings = std::dynamic_pointer_cast<SimpleSinBlockSettings>(settings);
		if (!m_settings)
		{
			return false;
		}

		WaveFormatHeader format;
		format.channels = 1;
		format.samplesPerSec = m_settings->sampleRate;
		format.bitsPerSample = 32;
		format.audioTag = 3;
		format.avgBytesPerSec = format.channels * format.samplesPerSec * (format.bitsPerSample / 8);
		//TODO: fill this
		//format.blockAlign;
		//format.format;
		const bool result = m_result.initialize(m_settings->bufferSize, format);
		reset();

		return result;
	}

	void finalize()override
	{
		m_result.clearData();
	}

	void stopLoop()override
	{
		//TODO: propagate to children
	}

	void reset()override
	{
		m_phase = m_settings->startPhase;
		m_elapsedTime = 0;
		m_gain = m_settings->defaultGain;
		m_freq = m_settings->defaultFreq;
		//TODO: propagate to children
	}

	bool isFinished()override
	{
		if (m_settings->time < 0)
		{
			return false;
		}

		return m_settings->time <= m_elapsedTime;
	}

	AudioBuffer* getResult()override
	{
		return &m_result;
	}

	void update() override
	{
		const double twoPIovrSampleRate = 2 * M_PI / static_cast<double>(m_result.getFormat().samplesPerSec);
		float* resultData = m_result.getData();
		//TODO: use m_elapsedTime
		UInt32 framesToGenerate = m_result.getMaxFrames();
		for (UInt32 i = 0; i < framesToGenerate; ++i)
		{
			resultData[i] = static_cast<float>(m_gain * sin(m_phase));
			increasePhase(twoPIovrSampleRate);
		}
		m_result.setActualFrames(framesToGenerate);
	}
private:
	void increasePhase(const double twoPIovrSampleRate)
	{
		m_phase += twoPIovrSampleRate * m_freq;
		if (m_phase >= 2 * M_PI)
		{
			m_phase -= 2 * M_PI;
		}
		if (m_phase < 0.0)
		{
			m_phase += 2 * M_PI;
		}
	}
	double m_elapsedTime;
	double m_phase;
	double m_gain;
	double m_freq;
	AudioBuffer m_result;
	std::shared_ptr<SimpleSinBlockSettings> m_settings;
};

class IAudioSource
{
public:
	virtual ~IAudioSource() {}

	virtual bool initialize(const SourceID sourceId, const std::shared_ptr<AudioBlockSettings>& settings) = 0;
	virtual void finalize() = 0;
	virtual bool play() = 0;
	virtual void pause() = 0;
	virtual void stop() = 0;
	virtual void stopLoop() = 0;
	virtual void update() = 0;
	virtual bool isFinished()const = 0;
	virtual SourceID getSourceId()const = 0;
	virtual AudioBuffer* getResult() = 0;
};

class SoundSource : public IAudioSource
{
public:
	SoundSource():
		m_isPlaying(false),
		m_isFinished(false),
		m_sourceId(0),
		m_block(nullptr)
	{
	}

	SourceID getSourceId()const override
	{
		return m_sourceId;
	}

	bool initialize(const SourceID sourceId, const std::shared_ptr<AudioBlockSettings>& settings)override
	{
		if (m_block)
		{
			return false;
		}

		m_block = std::make_shared<SimpleSinBlock>();
		if (!m_block)
		{
			return false;
		}

		return m_block->initialize(settings);
	}

	void finalize()override
	{
		m_block = nullptr;
	}

	bool play()override
	{
		if (m_isFinished)
		{
			return false;
		}

		if (m_isPlaying)
		{
			return false;
		}

		m_isPlaying = true;

		return true;
	}

	void pause()override
	{
		m_isPlaying = false;
	}

	void stop()override
	{
		m_isPlaying = false;
		m_block->reset();
	}

	void stopLoop()override
	{
		m_block->stopLoop();
	}

	void update()override
	{
		if (!m_isPlaying)
		{
			return;
		}

		m_block->update();

		m_isFinished = m_block->isFinished();
		if (m_isFinished)
		{
			m_isPlaying = false;
		}
	}

	bool isFinished()const override
	{
		return m_isFinished;
	}

	AudioBuffer* getResult()override
	{
		return m_block->getResult();
	}

private:
	bool m_isPlaying;
	bool m_isFinished;
	SourceID m_sourceId;
	std::shared_ptr<IAudioBlock> m_block;
};

class IAudioObject
{
public:
	virtual ~IAudioObject() {}
	//TODO: usefull setting is except
	virtual bool playAll() = 0;
	virtual void pauseAll() = 0;
	virtual void stopAll() = 0;
	virtual void stopLoopAll() = 0;
	virtual void update() = 0;
	// virtual AudioBuffer* getResult() = 0;
};

class AudioBus : public IAudioObject
{
public:
	void update() override
	{
		//if it will has children
		//mix all
	}
};

class AudioOutput : public IAudioObject
{
	void update() override
	{
		//mix all buffers and push it in the queue
	}
};

class AudioSourceTemplate : public IAudioObject
{
public:
	AudioSourceTemplate():
		m_settings(nullptr)
	{
	}

	bool setSettings(const std::shared_ptr<AudioBlockSettings>& settings)
	{
		if (!settings)
		{
			return false;
		}

		m_settings = settings;

		return true;
	}

	bool createSound()
	{
		if (!m_settings)
		{
			return false;
		}

		std::shared_ptr<IAudioSource> newSound = std::make_shared<SoundSource>();
		if (!newSound)
		{
			return false;
		}

		if (!newSound->initialize(m_nextSourceId, m_settings))
		{
			return false;
		}

		++m_nextSourceId;

		//m_sources.pushFront(newSound);
		return true;
	}

	void update()override
	{
		m_sources.removeIf([](IAudioSource const & sound) {return sound.isFinished(); });

		//m_sources.forEach([](IAudioSource & sound) { sound.update(); });

		//All buffers has equal sizes
		m_sources.forEach([&](IAudioSource & sound)
		{
			sound.update();
			m_result.mixData(sound.getResult());
		});
	}

	bool play(SourceID id)
	{
		bool result = true;
		m_sources.forEach([&](IAudioSource & sound)
		{
			if(sound.getSourceId() == id)
			{
				if (!sound.play())
				{
					result = false;
				}
			}
		});

		return result;
	}

	void pause(SourceID id)
	{
		m_sources.forEach([&](IAudioSource & sound)
		{
			if (sound.getSourceId() == id)
			{
				sound.pause();
			}
		});
	}

	void stop(SourceID id)
	{
		m_sources.forEach([&](IAudioSource & sound)
		{
			if (sound.getSourceId() == id)
			{
				sound.stop();
			}
		});
	}

	void stopLoop(SourceID id)
	{
		m_sources.forEach([&](IAudioSource & sound)
		{
			if (sound.getSourceId() == id)
			{
				sound.stopLoop();
			}
		});
	}

	bool playAll()override
	{
		bool result = true;
		m_sources.forEach([&](IAudioSource & sound) { if(!sound.play()) result = false; });

		return result;
	}

	void pauseAll()override
	{
		m_sources.forEach([&](IAudioSource & sound) { sound.pause(); });
	}

	void stopAll()override
	{
		m_sources.forEach([&](IAudioSource & sound) { sound.stop(); });
	}

	void stopLoopAll()override
	{
		m_sources.forEach([&](IAudioSource & sound) { sound.stopLoop(); });
	}
private:
	std::shared_ptr<AudioBlockSettings> m_settings;
	ListThreadsafe<IAudioSource> m_sources;
	AudioBuffer m_result;
	static SourceID m_nextSourceId;
};

SourceID AudioSourceTemplate::m_nextSourceId = 0;
//TODO: it should be singleton
class AudioManager
{
public:
	AudioManager():
		m_updateTimeMs(1000),
		m_thread(std::bind(&AudioManager::update, this))
	{

	}

	//TODO: pass AudioManagerSettings
	bool initialize()
	{
		//TODO: setup m_updateTimeMs here
		return false;
	}

	void finalize()
	{
		m_thread.interrupt();
		m_thread.join();
		m_audioObjects.clear();
	}

	//TODO: add tree and methods for managing it

	void update()
	{
		while (true)
		{
			const auto startUpdate = std::chrono::high_resolution_clock::now();

			for (auto it : m_audioObjects)
			{
				it->update();
				interruptionPoint();
			}
			const auto endUpdate = std::chrono::high_resolution_clock::now();
			const auto updateTime = endUpdate - startUpdate;
			//TODO: use condition variable here
			std::this_thread::sleep_for(std::chrono::milliseconds(m_updateTimeMs) - updateTime);
			interruptionPoint();
		}
	}

	bool createSourceInstance(const UInt32 templateId)
	{
		//TODO: checks
		AudioSourceTemplate* sourceTemplate = m_sourceTemplate[templateId];
		return sourceTemplate->createSound();
	}
private:
	UInt32 m_updateTimeMs;
	//It can be not threadsafe list
	//If we change position in list we can use mutex. else we can use atomic ptr
	std::list<std::shared_ptr<IAudioObject>> m_audioObjects;
	//TODO: use hash table
	std::vector<AudioSourceTemplate*> m_sourceTemplate; //just pointers to objects stored in m_audioObjects
	InterruptibleThread m_thread;
};

void main()
{
	AudioManager manager;
	manager.initialize();
	std::this_thread::sleep_for(std::chrono::milliseconds(4500));
	printf("Program stop!\n");
	manager.finalize();
	printf("Program end!\n");
}
