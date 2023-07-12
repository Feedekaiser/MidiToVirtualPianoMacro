// Dear Microsoft, if malloc failed, then it is not my fault but the users' 


#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

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
		YieldProcessor();
}


const char* FILE_NAME = "song.txt";
const unsigned int INPUT_BUFFER_SIZE = 16; // does not mean it can press INPUT_BUFFER_SIZE many keys. Must include shift.

struct Note
{
	int interval; 
	int length;
	INPUT inputs[INPUT_BUFFER_SIZE * 2];
};

inline int is_capital(char c)
{
	return (c >= 'A' && c <= 'Z');
}

inline int is_shifted(char c)
{
	return is_capital(c) || !(c >= 'a' && c <= 'z');
}

inline int is_not_capital(char c)
{
	return !is_capital(c);
}

inline void set_virtual_key(INPUT *input, WORD value, WORD flag = 0)
{
	input->type = INPUT_KEYBOARD;
	input->ki.wVk = value;
	input->ki.dwFlags = flag;
}

void read_line(Note *note, const char* line)
{
	int i = 0;
	for (; line[i] != ' '; ++i)
		note->interval = note->interval * 10 + (line[i] - '0');

	if (line[++i] == '=')
		return;

	char buffer_shifted_keys[INPUT_BUFFER_SIZE] { 0 };
	char buffer_unshifted_keys[INPUT_BUFFER_SIZE] { 0 };

	int  keys_total   = 0;
	int  keys_shifted = 0;

	for (; line[i] != '\n' && line[i] != '\0'; ++i)
	{

		int shifted = is_shifted(line[i]);
		char* buffer;
		int index;

		if (shifted)
		{
			buffer = buffer_shifted_keys;
			index = keys_shifted;
		}
		else
		{
			buffer = buffer_unshifted_keys;
			index = keys_total - keys_shifted;
		}

		buffer[index] = line[i];

		++keys_total;
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
			WORD virtual_key_value = map.find(c) != map.end() ? map.find(c)->second : c;

			set_virtual_key(&note->inputs[i * 2 + 1], virtual_key_value);
			set_virtual_key(&note->inputs[i * 2 + 2], virtual_key_value, KEYEVENTF_KEYUP);
		}
	}


	int inputs_start_index = ((keys_shifted > 0) + keys_shifted) * 2;
	for (int i = 0; i < keys_total - keys_shifted; ++i)
	{
		WORD virtual_key_value = buffer_unshifted_keys[i] - ('a' - 'A');
		set_virtual_key(&note->inputs[inputs_start_index + i * 2], virtual_key_value);
		set_virtual_key(&note->inputs[inputs_start_index + i * 2 + 1], virtual_key_value, KEYEVENTF_KEYUP);
	}
}

int main(void)
{
#pragma comment(lib, "winmm.lib")
	timeBeginPeriod(PERIOD);

	std::vector<Note> notes;

	{
		FILE* file = fopen(FILE_NAME, "r");

		if (!file)
		{
			printf("Failed opening %s \nExiting...", FILE_NAME);
			getchar();
			return -1;
		}

		printf("Sucessfully opened file.\nBegin Reading...\n");


		char buffer[64];
		while (fgets(buffer, 64, file))
		{
			Note note = {};
			read_line(&note, buffer);
			notes.push_back(note);
		}
		notes.shrink_to_fit();


		// mysteriously, this corrupts the data. this however do not copy so it is more efficient
		//for (; fgets(buffer, 64, file);)
		//{
		//	read_line(notes + size, buffer);
		//	if (++size >= capacity)
		//	{
		//		capacity *= 2;
		//		Note* notes_temp = (Note*)realloc(notes, capacity * sizeof(Note));
		//	}
		//}
		fclose(file);
	}

	printf("loaded the file.\n");
	for (;;)
	{
		int i = 0;
		bool play = false;
		printf("Press DEL to start, DEL again to stop, and END to reset.\n");
		for (;;)
		{
			if (play)
			{
				Note* note = &notes[i++];
				if (note->interval > 0) robustSleep(note->interval);
				if (note->length > 0) SendInput(note->length, note->inputs, sizeof(INPUT));
				

				if (i >= notes.size())
				{
					printf("finished playing\n");
					break;
				}
			}
			else robustSleep(16);

			if (GetAsyncKeyState(VK_DELETE) & 0x8000)
			{
				if (play)
					printf("stopped\n");
				play = !play;

				printf("play: %d", play);
				robustSleep(500); // extremely lazy fix to stop it from happening twice in a row and not stopping it.
			}

			if (GetAsyncKeyState(VK_END) & 0x8000)
			{
				i = 0;
				printf("reset\n");
			}
		}
	}
}
