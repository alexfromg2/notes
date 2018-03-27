#include "interruptible_thread.h"
#include <list>
#include "lock_list.h"


class AudioBuffer
{
public:
	bool mixData(AudioBuffer* otherBuffer)
	{
		return true;
	}
};

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