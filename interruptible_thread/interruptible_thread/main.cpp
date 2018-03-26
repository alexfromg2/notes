#include "interruptible_thread.h"


class AudioManager
{
public:
	AudioManager():
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
	}

	void update()
	{
		//interruptable
		/*auto nextUpdateTime = std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(m_updateTimeMs);
		removeFinished();
		for (auto it = m_audioObjects.begin(); it != m_audioObjects.end(); it++)
		{
			it->update();
		}
		std::this_thread::sleep_until(nextUpdateTime);*/
		while (true)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			printf("Hello world!\n");
			interruptionPoint();
		}
		//auto endUpdateTime = std::chrono::high_resolution_clock::now();
		//auto updateTime = std::chrono::duration<double, std::chrono::milliseconds>(endUpdateTime - startUpdateTime).count();
		//TODO: calc next update start time
		//TODO: wait here until(next update start time)
	}

	bool createSound()
	{
		/*
		Создаем звук в контейнере звуков.
		Это происходит в главном потоке.
		*/
	}
	bool setBufferSize(unsigned int bufferTimeSize)
	{
		m_updateTimeMs = bufferTimeSize;
	}
private:
	void removeFinished()
	{
		/*for (auto audioObjectsIter = m_audioObjects.begin(); audioObjectsIter != m_audioObjects.end(); ++audioObjectsIter)
		{
			audioObjectsIter->removeFinished();
		}*/
	}
private:
	unsigned int m_updateTimeMs;
	//std::list<IAudioObject> m_audioObjects;
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

/*
class AudioBuffer
{

};

class IAudioObject
{
public:
virtual void update()=0;
virtual void removeFinished() = 0;
virtual AudioBuffer* getResult() = 0;
};

#include "lock_list.h"
class AudioSourceBlock: public IAudioObject
{
public:
bool setSettings();//settings for creation
bool createSound()
{
std::shared_ptr<IAudioSource> newSound = std::make_shared<IAudioSource>();
//creation code here
m_sources.pushFront(newSound);
}
void update()override
{
m_sources.forEach([](IAudioSource& sound) { sound.update(); });
bool wasInitialized = false;
m_sources.forEach([&wasInitialized](IAudioSource& sound) { sound.update(); if (!wasInitialized) {
m_result->initByData(sound.getResult()); wasInitialized = true;
}
else { m_result->mixData(sound.getResult()); } });
}
void removeFinished()override
{
m_sources.removeIf([](IAudioSource const& sound) {return sound.isStopped(); });
}
private:
ListThreadsafe<IAudioSource> m_sources;//sources in threadsafe list
std::shared_ptr<AudioBuffer> m_result;
};

#include <chrono>
#include <thread>
*/