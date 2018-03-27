#include "interruptible_thread.h"
#include <cstdint>
#include <list>
#include "lock_list.h"


//TODO: use this types
typedef uint8_t UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;

typedef float Float32;

struct WaveFormatHeader
{
	char format[4];//like WAVE OR AVI
				   //TODO support this
				   //http://audiocoding.ru/article/2008/05/22/wav-file-structure.html
	unsigned short audioTag;//2 bytes
	unsigned short channels;//2 bytes
	unsigned int samplesPerSec;//4 bytes
	unsigned int avgBytesPerSec;//4 bytes
	unsigned short blockAlign;//2 bytes
	unsigned short bitsPerSample;//2 bytes
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

	inline bool initialize(unsigned int max_frames, const WaveFormatHeader& bufferFormat)
	{
		//TODO: check buffer format;
		if (max_frames == 0)
			return false;

		clearData();

		m_maxFrames = max_frames;
		m_bufferFormat = bufferFormat;

		m_data = new float[m_maxFrames*m_bufferFormat.channels];
		if (m_data)
			return true;

		return false;
	}

	inline bool HasData()
	{
		return (nullptr != m_data);
	}

	float* getData()
	{
		return m_data;
	}

	bool mixData(AudioBuffer* otherBuffer)
	{
		//TODO: support conversion between different buffer formats
		if (!(HasData() && otherBuffer && otherBuffer->HasData()))
			return false;

		if (m_maxFrames < otherBuffer->getActualFrames())
			return false;

		unsigned int mixPart = (otherBuffer->getActualFrames() < m_actualFrames) ? otherBuffer->getActualFrames() : m_actualFrames;
		unsigned int i = 0;

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

	inline unsigned int getMaxFrames() { return m_maxFrames; }
	inline unsigned int getActualFrames() { return m_actualFrames; }
	inline void setActualFrames(unsigned int actualFrames) { m_actualFrames = actualFrames; }

protected:

	float*          m_data;

	WaveFormatHeader m_bufferFormat;

	unsigned int        m_maxFrames;
	unsigned int        m_actualFrames;
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
	virtual void removeFinished() = 0;
	// virtual AudioBuffer* getResult() = 0;
};


class AudioSourceTemplate : public IAudioObject
{
public:
	bool setSettings();//settings for creation

	bool createSound()
	{
		std::shared_ptr<IAudioSource> newSound = std::make_shared<IAudioSource>();
		//creation code here
		m_sources.pushFront(newSound);
		return true;
	}

	void update()override
	{
		m_sources.forEach([](IAudioSource & sound) { sound.update(); });

		//All buffers has equal sizes
		m_sources.forEach([&](IAudioSource & sound)
		{
			sound.update();
			m_result->mixData(sound.getResult());
		});
	}
	void removeFinished()override
	{
		m_sources.removeIf([](IAudioSource const & sound) {return sound.isFinished(); });
	}
private:
	ListThreadsafe<IAudioSource> m_sources;
	//TODO: Why shared ptr?
	std::shared_ptr<AudioBuffer> m_result;
};


class AudioManager
{
public:
	AudioManager():
		m_updateTimeMs(1000),
		m_thread(std::bind(&AudioManager::update, this))
	{

	}

	bool initialize()
	{
		return false;
	}

	void finalize()
	{
		m_thread.interrupt();
		m_thread.join();
		m_audioObjects.clear();
	}

	void update()
	{
		while (true)
		{
			const auto startUpdate = std::chrono::high_resolution_clock::now();
			removeFinished();
			interruptionPoint();
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

	bool createSourceInstance(unsigned int templateId)
	{
		//TODO: checks
		AudioSourceTemplate* sourceTemplate = m_sourceTemplate[templateId];
		return sourceTemplate->createSound();
	}

	bool setBufferSize(unsigned int bufferTimeSize)
	{
		m_updateTimeMs = bufferTimeSize;
	}
private:
	void removeFinished()
	{
		for (auto it : m_audioObjects)
		{
			it->removeFinished();
		}
	}
private:
	unsigned int m_updateTimeMs;
	//It can be not threadsafe list
	//If we change position in list we can use mutex. else we can use atomic ptr
	std::list<std::shared_ptr<IAudioObject>> m_audioObjects;
	//TODO: use hash table
	//TODO: store sound template class
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