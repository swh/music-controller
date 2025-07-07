#define NUM_TESTS 4

typedef struct {
	char *artist;
	char *album;
	char *title;
	int length;
} TestData;

int test_num = 0;
TestData test_data[NUM_TESTS] = {
  {"Einstürzende Neubauten", "Tabula Rasa", "Headcleaner", 10*(4*60+44)},
  {"Front 242", "Front by front", "Headhunter v2.3", 10*(60*93+23)},
  {"foo", "bar", "baz", 11},
  {"Really long band name, lets see what happens to this!", "Σύγχρονος Άγγελος “Modern Angel”", "«void»", 10 * 523}
};
long test_trigger = 0;