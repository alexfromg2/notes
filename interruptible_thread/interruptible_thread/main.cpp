#include "interruptible_thread.h"
#include <cstdint>
#include <list>
#include "lock_list.h"


typedef uint8_t UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;

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

/*class AudioSourceSettings
{
public:

};*/

class IAudioSource
{
public:
	virtual ~IAudioSource() {}

	virtual void update() = 0;
	virtual bool isFinished()const = 0;
	virtual AudioBuffer* getResult() = 0;
};

class IAudioObject
{
public:
	virtual ~IAudioObject() {}
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
	bool setSettings();//settings for creation

	bool createSound()
	{
		//std::shared_ptr<IAudioSource> newSound = std::make_shared<IAudioSource>();
		//creation code here
		//m_sources.pushFront(newSound);
		return true;
	}

	void update()override
	{
		removeFinished();

		m_sources.forEach([](IAudioSource & sound) { sound.update(); });

		//All buffers has equal sizes
		m_sources.forEach([&](IAudioSource & sound)
		{
			sound.update();
			m_result->mixData(sound.getResult());
		});
	}

	void removeFinished()
	{
		m_sources.removeIf([](IAudioSource const & sound) {return sound.isFinished(); });
	}
private:
	ListThreadsafe<IAudioSource> m_sources;
	//TODO: Why shared ptr?
	std::shared_ptr<AudioBuffer> m_result;
};


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