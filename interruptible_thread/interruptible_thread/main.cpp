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

/*class AudioSourceSettings
{
public:

};*/

class IAudioBlock
{
public:
	virtual ~IAudioBlock() {}
	virtual void stopLoop() = 0;
	virtual void reset() = 0;
	virtual bool isFinished() = 0;
	virtual AudioBuffer* getResult() = 0;
	virtual void update() = 0;
};

class IAudioSource
{
public:
	virtual ~IAudioSource() {}

	virtual void update() = 0;
	virtual bool isFinished()const = 0;
	virtual AudioBuffer* getResult() = 0;
};

class SoundSource : public IAudioSource
{
public:
	bool play()
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
	}

	void pause()
	{
		m_isPlaying = false;
	}

	void stop()
	{
		m_isPlaying = false;
		//m_block->reset();
	}

	void stopLoop()
	{
		//m_block->stopLoop();
	}

	void update()
	{
		if (!m_isPlaying)
		{
			return;
		}

		//m_block->update();
		//m_isFinished = m_block->isFinished();
		if (m_isFinished)
		{
			m_isPlaying = false;
		}
	}

	bool isFinished()const
	{
		return m_isFinished;
	}

	AudioBuffer* getResult()
	{
		return nullptr;//m_block->getResult()
	}

private:
	bool m_isPlaying;
	bool m_isFinished;
	//std::shared_ptr<IAudioBlock> m_block;
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

class SimpleSinBlockSettings
{
public:
	double startPhase;
	float defaultGain;
	float defaultFreq;
	float time;
	UInt32 sampleRate;
};

#define _USE_MATH_DEFINES

#include <math.h>
class SimpleSinBlock: public IAudioBlock
{
public:
	void update() override
	{
		AudioBuffer* parameter1;
		AudioBuffer* parameter2;
		auto parameter1data = parameter1->getData();
		auto parameter2data = parameter2->getData();
		/*if (resultBufferSize == 0 || inputs[1].getBuffer() == nullptr || inputs[2].getBuffer() == nullptr)
		{
			return;
		}*/

		const double twoPIovrSampleRate = 2 * M_PI / static_cast<double>(m_result.getFormat().samplesPerSec);
		auto resultData = m_result.getData();
		for (UInt32 i = 0; i < m_result.getMaxFrames(); ++i)// TODO: get from result buffer
		{
			resultData[i] = parameter2data[i] * static_cast<float>(sin(m_phase));
			increasePhase(twoPIovrSampleRate, i, parameter2data);
		}
	}
private:
	void increasePhase(const double twoPIovrSampleRate, const UInt32 i, float* parameter1)
	{
		m_phase += twoPIovrSampleRate * parameter1[i];
		if (m_phase >= 2 * M_PI)
		{
			m_phase -= 2 * M_PI;
		}
		if (m_phase < 0.0)
		{
			m_phase += 2 * M_PI;
		}
	}
	double m_phase;
	AudioBuffer m_result;
};
/*
class SimpleSinAct :public MultiLinkedObject
{
public:
SimpleSinAct();

~SimpleSinAct();

bool setInput(unsigned int inputID, FloatParameter* val);

void Process()override;

protected:
float* resultBuffer;
double phase;
float defaultGain;
float defaultFreq;
//Global for now
unsigned long sampleRate;
unsigned long resultBufferSize;

void increasePhase(double twoPIovrSampleRate, ParameterBufferSize i);
};

SimpleSinAct::~SimpleSinAct()
{
}

bool SimpleSinAct::setInput(unsigned int inputID, FloatParameter* val)
{
if (inputs.size() <= inputID || val == nullptr)
return false;

if (inputID == 0 && (val == nullptr || val->getBufferSize() != 1 || val->getValue(0) < 1))
return false;

bool result = inputs[inputID].setValue(val->getBuffer(), val->getBufferSize());
if (inputID == 0 && result)
{
resultBufferSize = static_cast<unsigned long>(val->getValue(0));
if (outputs[0].getBufferSize() != resultBufferSize)
{
if (resultBuffer)
delete[] resultBuffer;

resultBuffer = new float[resultBufferSize];
outputs[0].setValue(resultBuffer, resultBufferSize);
}
}
return result;
}

void SimpleSinAct::Process()
{
if (resultBufferSize == 0 || inputs[1].getBuffer() == nullptr || inputs[2].getBuffer() == nullptr)
return;
double twoPIovrSampleRate = TWO_PI / (double)sampleRate;
for (ParameterBufferSize i = 0; i < resultBufferSize; ++i)
{
resultBuffer[i] = inputs[2].getValue(i) * static_cast<float>(sin(phase));
increasePhase(twoPIovrSampleRate, i);
}
}

void SimpleSinAct::increasePhase(double twoPIovrSampleRate, ParameterBufferSize i)
{
phase += twoPIovrSampleRate * inputs[1].getValue(i);
if (phase >= TWO_PI)
phase -= TWO_PI;
if (phase < 0.0)
phase += TWO_PI;
}
*/