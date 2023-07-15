#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#pragma comment(lib, "winmm.lib")

#include <windows.h>
#include <stdio.h>
#include <unordered_map>
#include <thread>
#include <chrono>

#define PERIOD 1
#define TOLERANCE 0.02

/* https://blog.bearcats.nl/perfect-sleep-function/ modified to use millisecond */
void robustSleep(int millisecond)
{
	using namespace std;
	using namespace chrono;

	auto t0 = high_resolution_clock::now();
	auto target = t0 + nanoseconds(int64_t(millisecond * 1e6));

	// sleep
	double ms = millisecond - (PERIOD + TOLERANCE);
	int ticks = (int)(ms / PERIOD);
	if (ticks > 0)
		this_thread::sleep_for(milliseconds(ticks * PERIOD));

	// spin
	while (high_resolution_clock::now() < target)
		_mm_pause();
}


const char* FILE_NAME = "song.txt";
const unsigned int INPUT_BUFFER_SIZE = 16; // does not mean it can press INPUT_BUFFER_SIZE many keys. Must include shift.

struct Note
{
	int interval;
	int length;
	INPUT inputs[INPUT_BUFFER_SIZE * 2];
};


inline int is_not_shifted(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

inline void set_virtual_key(INPUT* input, WORD value, WORD flag = 0)
{
	input->type = INPUT_KEYBOARD;
	input->ki.wVk = value;
	input->ki.dwFlags = flag;
}

void read_line(Note* note, const char* line)
{
	int i = 0;
	note->interval = 0;
	for (; line[i] != ' '; ++i)
		note->interval = note->interval * 10 + (line[i] - '0');

	if (line[++i] == '=') { note->length = 0; return; }
		
	char buffer_shifted_keys[INPUT_BUFFER_SIZE]{ 0 };
	char buffer_unshifted_keys[INPUT_BUFFER_SIZE]{ 0 };

	int  keys_total = 0;
	int  keys_shifted = 0;

	for (; line[i] != '\n'; ++keys_total, ++i)
	{
		int shifted = !is_not_shifted(line[i]);
		char* buffer = shifted ? buffer_shifted_keys : buffer_unshifted_keys; // possible branchhless by combining both arrays into 1 and calculate the offset in the index instead.
		int index = shifted ? keys_shifted : keys_total - keys_shifted;

		buffer[index] = line[i];
		keys_shifted += shifted;
	}

	note->length = (keys_total + (keys_shifted > 0)) * 2;

	if (keys_shifted > 0)
	{
		set_virtual_key(&note->inputs[0], VK_LSHIFT);
		set_virtual_key(&note->inputs[keys_shifted * 2 + 1], VK_LSHIFT, KEYEVENTF_KEYUP);

		// using std::set and an array MIGHT be faster. idk. 
		// using just array MIGHT be even FASTER. idk.
		static std::unordered_map<char, WORD> map = {
			{'!', 0x31}, {'@', 0x32}, {'#', 0x33},
			{'$', 0x34}, {'%', 0x35}, {'^', 0x36},
			{'&', 0x37}, {'*', 0x38}, {'(', 0x39},
			{'0', 0x30}
		};

		for (int i = 0; i < keys_shifted; ++i)
		{
			char c = buffer_shifted_keys[i];
			WORD virtual_key_value = (c < 'A' || c > 'Z') ? map.find(c)->second : c;

			set_virtual_key(&note->inputs[i * 2 + 1], virtual_key_value);
			set_virtual_key(&note->inputs[i * 2 + 2], virtual_key_value, KEYEVENTF_KEYUP);
		}
	}

	int inputs_start_index = ((keys_shifted > 0) + keys_shifted) * 2;
	for (int i = 0; i < keys_total - keys_shifted; ++i)
	{
		char c = buffer_unshifted_keys[i];
		WORD virtual_key_value = buffer_unshifted_keys[i] - ('a' - 'A') * (c > '9');
		set_virtual_key(&note->inputs[inputs_start_index + i * 2], virtual_key_value);
		set_virtual_key(&note->inputs[inputs_start_index + i * 2 + 1], virtual_key_value, KEYEVENTF_KEYUP);
	}
}

int get_notes(Note** notes)
{
	FILE* file = fopen(FILE_NAME, "r");

	if (!file)
	{
		printf("Failed opening %s \nExiting...", FILE_NAME);
		getchar();
		exit(-1);
	}

	printf("Sucessfully opened file.\nBegin Reading...\n");

	char buffer[32];
	int note_count = -1;
	int capacity = 256;

	*notes = (Note*)malloc(capacity * sizeof(Note));

	while (fgets(buffer, 32, file))
	{
		if (++note_count >= capacity)
			*notes = (Note*)realloc(*notes, (capacity *= 2) * sizeof(Note));

		read_line(&((*notes)[note_count]), buffer);
	}
	++note_count;

	*notes = (Note*)realloc(*notes, note_count * sizeof(Note));

	fclose(file);

	return note_count;
}

int main(void)
{
	timeBeginPeriod(PERIOD);
	Note* notes;
	const int notes_size = get_notes(&notes);	

	printf("loaded the file.\n");
	for (;;)
	{
		int i = -1;
		bool play = false;
		printf("Press DEL to start, DEL again to stop, and END to reset.\n");
		while (i < notes_size)
		{
			if (play)
			{
				Note* note = &notes[++i];
				SendInput(note->length, note->inputs, sizeof(INPUT));
				robustSleep(note->interval);
			}
			else robustSleep(16);

			if (GetAsyncKeyState(VK_DELETE) & 0x8000)
			{
				play = !play;
				printf(play ? "starting\n" : "pausing\n");
				robustSleep(500); // extremely lazy fix to stop it from happening twice in a row and not stopping it.
			}
		}

		printf("finished playing\n");
	}

	return 0;
}
