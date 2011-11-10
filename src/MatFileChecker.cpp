#include <iostream>
#include <stdint.h>

using namespace std;

int main(int argc, char *argv[]) {

	if(argc < 1) {
		cout << "No filename given." << endl;
		return -1;
	}

	fstream matfile(argv[1], ios_base::in | ios_base::binary);
	if(!matfile.is_open()) {
		cout << "Error opening file " << argv[1] << endl;
		return -1;
	}

	// Read matlab header
	char header[128];
	fstream.read(header, 128);

	cout << "Version = " << uint16_t(header+124) << endl;
	cout << "Endianness = " << uint16_t(hdeader+126) << endl;

	while(!fstream.eof()) {
		uint32_t data_head[2];
		fstream.read((char*)data_head, 2*sizeof(uint32_t));

		cout << "Data block of size " << data_head[1] << " and type " << data_head[0] << endl;

		char *buffer = new char[data_head[1]];

		fstream.read(buffer, data_head[1]);

	}
}