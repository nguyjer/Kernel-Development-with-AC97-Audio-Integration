#include "process.h"
#include "physmem.h"
#include "debug.h"
#include "vmm.h"
#include "pit.h"
#include "libk.h"

// id encoding
//   upper bit -> sign
//   3 bits -> kind
//   28 bits -> index
#define AC97_NAM_IO_VARIABLE_SAMPLE_RATE_FRONT_DAC 0x2C
#define AC97_NAM_IO_VARIABLE_SAMPLE_RATE_SURR_DAC 0x2E
#define AC97_NAM_IO_VARIABLE_SAMPLE_RATE_LFE_DAC 0x30
#define AC97_NAM_IO_VARIABLE_SAMPLE_RATE_LR_ADC 0x32

constexpr static uint32_t FL = 0x00000000;
constexpr static uint32_t PROC = 0x10000000;
constexpr static uint32_t SEM = 0x20000000;
constexpr static uint32_t INDEX_MASK = 0x0FFFFFFF;
constexpr uint32_t BUFFER_SIZE = 65536; // 128 KB per buffer
constexpr uint32_t NUM_BUFFERS = 32;

Shared<Process> Process::kernelProcess = Shared<Process>::make(true);

void Process::init(void)
{
}

Process::Process(bool isInit)
{
	if (isInit)
	{
		Shared<File> f{new U8250File(new U8250())};
		files[0] = f;
		files[1] = f;
		files[2] = f;
	}
}

Process::~Process()
{
	gheith::delete_pd(pd);
}

uint32_t swapEndian(uint32_t value)
{
	return ((value >> 24) & 0xFF) |
		   ((value >> 8) & 0xFF00) |
		   ((value << 8) & 0xFF0000) |
		   ((value << 24) & 0xFF000000);
}

void printContents(uint32_t *buffer)
{
	for (int i = 0; i < 10; i++)
	{
		Debug::printf("%x\n", buffer[i]);
	}
}

void clear_memory(uint32_t memory, uint32_t length)
{
	uint32_t *mem32 = (uint32_t *)(memory);

	for (uint32_t i = 0; i < (length / 4); i++)
	{
		*mem32 = 0;
		mem32++;
	}

	uint8_t *mem8 = (uint8_t *)(mem32);

	for (uint32_t i = 0; i < (length % 4); i++)
	{
		*mem8 = 0;
		mem8++;
	}
}

void ac97_set_sample_rate(uint16_t sample_rate)
{
	// check if variable sample rate feature is present
	using namespace AC97;
	Debug::printf("sample rate = %d\n", sample_rate);
	// set same variable rate on all outputs
	outw(BAR0 + AC97_NAM_IO_VARIABLE_SAMPLE_RATE_FRONT_DAC, sample_rate);
	outw(BAR0 + AC97_NAM_IO_VARIABLE_SAMPLE_RATE_SURR_DAC, sample_rate);
	outw(BAR0 + AC97_NAM_IO_VARIABLE_SAMPLE_RATE_LFE_DAC, sample_rate);
	outw(BAR0 + AC97_NAM_IO_VARIABLE_SAMPLE_RATE_LR_ADC, sample_rate);

	Debug::printf("sample rate register = %u\n", inw(BAR0 + AC97_NAM_IO_VARIABLE_SAMPLE_RATE_FRONT_DAC));
}

void findWavHDR(Shared<File> file, WAVHeader *wavhdr)
{
	file->read(wavhdr, 16);
	// char *fmt = new char[4];
	Debug::printf("first fmt = %c%c%c%c\n", wavhdr->fmt[0], wavhdr->fmt[1], wavhdr->fmt[2], wavhdr->fmt[3]);

	while (wavhdr->fmt[0] != 'f' || wavhdr->fmt[1] != 'm' || wavhdr->fmt[2] != 't' || wavhdr->fmt[3] != ' ')
	{
		file->read(((char *)wavhdr) + 16, 4);
		file->seek(file->getOffset() + wavhdr->fmt_length);
		file->read(((char*)wavhdr) + 12, 4);
		Debug::printf("fmt = %c%c%c%c\n", wavhdr->fmt[0], wavhdr->fmt[1], wavhdr->fmt[2], wavhdr->fmt[3]);
	}
	file->read(((char *)wavhdr) + 16, 4);
	file->read(((char *)wavhdr) + 20, wavhdr->fmt_length + 4);
	Debug::printf("first data = %c%c%c%c\n", wavhdr->data[0], wavhdr->data[1], wavhdr->data[2], wavhdr->data[3]);
	
	while (wavhdr->data[0] != 'd' || wavhdr->data[1] != 'a' || wavhdr->data[2] != 't' || wavhdr->data[3] != 'a')
	{
		file->read(((char *)wavhdr) + 40, 4);
		file->seek(file->getOffset() + wavhdr->data_size);
		file->read(((char *)wavhdr) + 36, 4);
		Debug::printf("data = %c%c%c%c\n", wavhdr->data[0], wavhdr->data[1], wavhdr->data[2], wavhdr->data[3]);
	}
	file->read(((char *)wavhdr) + 40, 4);
}

uint32_t Process::fillBuffers(Shared<File> file)
{
	Debug::printf("Filling buffers...\n");

	WAVHeader *wavhdr = new WAVHeader;
	findWavHDR(file, wavhdr);

	// int num_buffers = wavhdr->data_size / BUFFER_SIZE;
	int num_buffers = 31;

	// check file header
	if (wavhdr->magic0 != 'W' || wavhdr->magic1 != 'A' || wavhdr->magic2 != 'V' || wavhdr->magic3 != 'E')
	{
		Debug::printf("*** Trying to play a non-WAV audio file.\n");
		return -1;
	}

	outl(AC97::BAR1 + 0x00, (uint32_t)(AC97::audio_buffers)); // Set the base address for Buffer Descriptor List
	outb(AC97::BAR1 + 0x05, (uint8_t)num_buffers);			  // set number of descriptor entries

	Debug::printf("Starting audio buffers... num = %d\n", num_buffers);

	for (int i = 0; i < num_buffers; i++)
	{
		// Debug::printf("Print audio buffer %x\n", audio_buffers[i].pointer);
		AC97::audio_buffers[i].length = wavhdr->sample_rate_eq;
		file->read((char *)AC97::audio_buffers[i].pointer, BUFFER_SIZE);
		// Debug::printf("Reading Data buffer... i = %x\n", *((uint32_t *)(audio_buffers[i].pointer)));
		//  printContents((uint32_t *) AC97::audio_buffers[i].pointer);
	}
	// Debug::printf("Finished Reading Data Buffer...\n");
	AC97::audio_buffers[num_buffers].length = wavhdr->sample_rate_eq;
	file->read((char *)AC97::audio_buffers[num_buffers].pointer, BUFFER_SIZE);
	Debug::printf("Buffers filled!!\n");
	Debug::printf("file_size = %d\n", wavhdr->file_size);
	Debug::printf("data_size = %d\n", wavhdr->data_size);
	Debug::printf("sample_rate = %d\n", wavhdr->sample_rate);
	Debug::printf("num channels = %d\n", wavhdr->num_channels);
	Debug::printf("bitsPerSample = %d\n", wavhdr->bitsPerSample);

	ac97_set_sample_rate(wavhdr->sample_rate);
	outl(AC97::BAR1 + 0xB, 0x2);

	return wavhdr->data_size / wavhdr->sample_rate_eq;
}

int Process::newSemaphore(uint32_t init)
{
	LockGuard<BlockingLock> lock{mutex};

	for (int i = 0; i < NSEM; i++)
	{
		auto p = sems[i];
		if (p == nullptr)
		{
			sems[i] = Shared<Semaphore>::make(init);
			return SEM | (i & INDEX_MASK);
		}
	}
	return -1;
}

Shared<Semaphore> Process::getSemaphore(int id)
{
	LockGuard<BlockingLock> g{mutex};

	int idx = id & INDEX_MASK;
	if ((idx >= NSEM) || ((id & 0xF0000000) != SEM))
	{
		return Shared<Semaphore>{};
	}
	return sems[idx];
}

void Process::clear_private()
{
	using namespace gheith;

	LockGuard<BlockingLock> g{mutex};

	delete_private(pd);
}

Shared<Process> Process::fork(int &id)
{
	LockGuard<BlockingLock> lock{mutex};

	int index = -1;

	for (auto i = 0; i < NCHILD; i++)
	{
		auto e = children[i];
		if (e == nullptr)
		{
			index = i;
			break;
		}
	}

	if (index == -1)
	{
		id = -1;
		return Shared<Process>{};
	}

	auto child = Shared<Process>::make(false);

	// copy the private portion of the address space
	for (unsigned pdi = 512; pdi < 1024; pdi++)
	{
		auto parent_pde = pd[pdi];
		if ((parent_pde & 1) == 0)
			continue;
		auto parent_pt = (uint32_t *)(parent_pde & 0xFFFFF000);

		auto child_pde = child->pd[pdi];
		auto child_pt = (uint32_t *)(child_pde & 0xFFFFF000);

		if ((child_pde & 1) == 0)
		{
			child_pt = (uint32_t *)PhysMem::alloc_frame();
			child->pd[pdi] = uint32_t(child_pt) | 7;
		}

		for (unsigned pti = 0; pti < 1024; pti++)
		{
			auto child_pte = child_pt[pti];
			if ((child_pte & 1) == 1)
				continue;
			auto parent_pte = parent_pt[pti];
			if ((parent_pte & 1) == 0)
				continue;
			auto parent_frame = parent_pte & 0xFFFFF000;
			// Debug::printf("fork: copying %x\n",(pdi << 22) | (pti << 12));
			auto child_frame = PhysMem::alloc_frame();
			// Debug::printf("fork: copying:%x, parent:%x, child:%x\n",(pdi << 22) | (pti << 12),parent_frame,child_frame);
			memcpy((void *)child_frame, (void *)parent_frame, PhysMem::FRAME_SIZE);
			child_pt[pti] = uint32_t(child_frame) | 7;
		}
	}

	// child->addressSpace->copyFrom(addressSpace);
	for (auto i = 0; i < NSEM; i++)
	{
		auto s = sems[i];
		if (s != nullptr)
		{
			child->sems[i] = s;
		}
	}
	for (auto i = 0; i < NFILE; i++)
	{
		child->files[i] = files[i];
	}

	children[index] = child->output;
	id = PROC | index;
	return child;
}

int Process::getSemaphoreIndex(int id)
{
	int kind = id & 0xF0000000;
	int index = id & INDEX_MASK;
	if (kind != SEM)
		return -1;
	if (index >= NSEM)
		return -1;
	return index;
}

int Process::getChildIndex(int id)
{
	int kind = id & 0xF0000000;
	int index = id & INDEX_MASK;
	if (kind != PROC)
		return -1;
	if (index >= NCHILD)
		return -1;
	return index;
}

int Process::getFileIndex(int id)
{
	int kind = id & 0xF0000000;
	int index = id & INDEX_MASK;
	if (kind != FL)
		return -1;
	if (index >= NFILE)
		return -1;
	return index;
}

int Process::close(int id)
{
	auto index = getSemaphoreIndex(id);

	if (index != -1)
	{
		auto sem = sems[index];
		if (sem == nullptr)
		{
			return -1;
		}
		sems[index] = nullptr;
		return 0;
	}

	index = getChildIndex(id);

	if (index != -1)
	{
		auto e = children[index];
		if (e == nullptr)
		{
			return -1;
		}
		children[index] = nullptr;
		return 0;
	}

	index = getFileIndex(id);

	if (index != -1)
	{
		auto e = files[index];
		if (e == nullptr)
		{
			return -1;
		}
		files[index] = nullptr;
		return 0;
	}

	return -1;
}

int Process::wait(int id, uint32_t *ptr)
{
	auto index = getChildIndex(id);
	if (index < 0)
		return index;
	auto e = children[index];
	if (e == nullptr)
		return -1;
	*ptr = e->get();
	children[index] = nullptr;
	return 0;
}
