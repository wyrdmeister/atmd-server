/*
 * Copyright (C) 2011 Michele Devetta @ LGM <michele.devetta@unimi.it>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* C++ MAT v5.0 library version 1.0
 *
 * This file must be used along with MatFile.h
 * See MatFile.h for help and changelog.
 */

#include "MatFile.h"

void MatFile::open(std::string &filename) {
	this->open(filename.c_str());
}

void MatFile::open(const char* filename) {

	file.open(filename, std::ios_base::out | std::ios_base::binary);

	if(file.is_open()) {
		char buffer[128];

		/* Format the timestamp */
		time_t current_time = time(NULL);
		char *date_str = ctime(&current_time);

		/* Fill header */
		memset(buffer, 0x20, 128);
		memcpy(buffer, MAT_HEADER, strlen(MAT_HEADER));
		memcpy(buffer+strlen(MAT_HEADER), date_str, strlen(date_str));
		*(uint8_t*)(buffer+124) = MAT_VERSION_1B;
		*(uint8_t*)(buffer+125) = MAT_VERSION_2B;
		*(uint16_t*)(buffer+126) = MAT_ENDIAN;

		file.write(buffer, 128);
	}
}

uint32_t MatMatrix::header_size() {
	uint32_t size = 0;

	size += 16; // Flags

	size += 16; // Dimensions

	if(array_name.length() <= 4)
		size += 8; // Array name in compressed format

	else
		size += 8 + ((array_name.length() % 8) ? ((array_name.length() / 8 + 1) * 8) : (array_name.length())); // Array name in extended format (aligned to 8 bytes)

	return size;
}

void MatMatrix::make_baseheader(uint8_t *&buffer, uint32_t &size) {
	/* First we compute the size of the header and we allocate the buffer */
	size = header_size();

	buffer = new uint8_t[size];
	memset(buffer, 0x00, size);
	uint32_t offset = 0;

	/* Fill the buffer with data */

	/* Flags */
	*(uint32_t *)(buffer+offset) = miUINT32;
	*(uint32_t *)(buffer+offset+4) = 8;
	*(uint32_t *)(buffer+offset+8) = (flags << 8) | (mat_class);
	offset += 16;

	/* Dimensions */
	*(uint32_t *)(buffer+offset) = miINT32;
	*(uint32_t *)(buffer+offset+4) = 8;
	*(uint32_t *)(buffer+offset+8) = rows;
	*(uint32_t *)(buffer+offset+12) = cols;
	offset += 16;

	/* Name */
	if(array_name.length() <= 4) {
		/* Compressed data format */
		*(uint32_t *)(buffer+offset) = (array_name.length() << 16) | (miINT8);
		memcpy(buffer+offset+4, array_name.c_str(), array_name.length());
		offset += 8;

	} else {
		/* Standard data format */
		*(uint32_t *)(buffer+offset) = miINT8;
		*(uint32_t *)(buffer+offset+4) = array_name.length();
		memcpy(buffer+offset+8, array_name.c_str(), array_name.length());
		offset += 8 + ((array_name.length() % 8) ? ((array_name.length() / 8 + 1) * 8) : (array_name.length()));
	}
}

void MatCellArray::resize(uint32_t nrows, uint32_t ncols) {
	if(cells.size() > 0) {
		if(nrows < rows)
			nrows = rows;
		if(ncols < cols)
			nrows = cols;
		if(nrows > rows || ncols > cols) {
			std::vector<MatMatrix*> temp = cells;
			cells.reserve(nrows*ncols);
			for(uint32_t i=0; i < rows*cols; i++)
				cells[i] = NULL;
			for(uint32_t i=rows*cols; i < nrows*ncols; i++)
				cells.push_back(NULL);

			/* Copy data to new array */
			for(uint32_t i = 0; i < cols; i++)
				for(uint32_t j = 0; j < rows; j++)
					cells[nrows*i+j] = temp[rows*i+j];
			rows = nrows;
			cols = ncols;
		}
	} else {
		cells.reserve(nrows*ncols);
		for(uint32_t i=0; i < nrows*ncols; i++)
			cells.push_back(NULL);
		rows = nrows;
		cols = ncols;
	}
}

MatMatrix *&MatCellArray::operator()(uint32_t i, uint32_t j) {
	if(i < rows && j < cols) {
		return cells[i+rows*j];
	} else {
		resize((i+1 > rows) ? i+1 : rows, (j+1 > cols) ? j+1 : cols);
		return cells[i+rows*j];
	}
}

int MatCellArray::write(MatFile &file) {
	if(file.IsOpen()) {

		/* First we make the base header */
		uint8_t *base_header;
		uint32_t base_size;
		make_baseheader(base_header, base_size);

		/* Second we write the miMatrix header and we save the file pointer. */
		uint32_t head[2];
		head[0] = miMATRIX;
		head[1] = 0;
		file.write((char *)head, 2*sizeof(uint32_t));
		int32_t start_pos = file.get_pos();

		/* Third we write the cell header */
		file.write((char *)base_header, base_size);

		for(uint32_t i = 0; i < cells.size(); i++) {
			if(cells[i]) {
				if(cells[i]->get_class_name() == std::string(typeid(double).name())) {
					MatVector<double> *matrix = dynamic_cast<MatVector<double>*>(cells[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(cells[i]->get_class_name() == std::string(typeid(float).name())) {
					MatVector<float> *matrix = dynamic_cast<MatVector<float>*>(cells[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(cells[i]->get_class_name() == std::string(typeid(uint32_t).name())) {
					MatVector<uint32_t> *matrix = dynamic_cast<MatVector<uint32_t>*>(cells[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(cells[i]->get_class_name() == std::string(typeid(int32_t).name())) {
					MatVector<int32_t> *matrix = dynamic_cast<MatVector<int32_t>*>(cells[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(cells[i]->get_class_name() == std::string(typeid(uint16_t).name())) {
					MatVector<uint16_t> *matrix = dynamic_cast<MatVector<uint16_t>*>(cells[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(cells[i]->get_class_name() == std::string(typeid(int16_t).name())) {
					MatVector<int16_t> *matrix = dynamic_cast<MatVector<int16_t>*>(cells[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(cells[i]->get_class_name() == std::string(typeid(uint8_t).name())) {
					MatVector<uint8_t> *matrix = dynamic_cast<MatVector<uint8_t>*>(cells[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(cells[i]->get_class_name() == std::string(typeid(int8_t).name())) {
					MatVector<int8_t> *matrix = dynamic_cast<MatVector<int8_t>*>(cells[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(cells[i]->get_class_name() == std::string("cell")) {
					MatCellArray *matrix = dynamic_cast<MatCellArray*>(cells[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(cells[i]->get_class_name() == std::string("struct")) {
					MatStruct *matrix = dynamic_cast<MatStruct*>(cells[i]);
					matrix->set_name("");
					matrix->write(file);

				} else {
					std::cerr << "Runtime error! MatCellArray::write requested an unexpected type (" << cells[i]->get_class_name() << ")." << std::endl;
					return -1;
				}

			} else {
				// We should write and empty array
				MatVector<double> matrix;
				matrix.set_class(mxDOUBLE_CLASS);
				matrix.write(file);
			}
		}

		/* In the end we get the data size */
		int32_t stop_pos = file.get_pos();
		head[1] = stop_pos - start_pos;

		file.set_pos(start_pos - 4);
		file.write((char *)(head+1), 4);

		file.set_pos(stop_pos);

		return 0;

	} else {
		return -1;
	}
}

void MatStruct::resize(uint32_t nrows, uint32_t ncols) {
	if(field_names.size()!=0) {
		if(field_data.size()!=0) {
			if(nrows < rows)
				nrows = rows;
			if(ncols < cols)
				nrows = cols;
			if(nrows > rows || ncols > cols) {
				/* First save the original vector into a temporary one */
				std::vector<MatMatrix*> temp = field_data;

				/* Second extend the original vector to the new size */
				field_data.reserve(nrows * ncols * field_names.size());
				for(uint32_t i = 0; i < rows*cols*field_names.size(); i++)
					field_data[i] = NULL;
				for(uint32_t i = rows*cols*field_names.size(); i < nrows*ncols*field_names.size(); i++)
					field_data.push_back(NULL);

				/* Third copy the original data into the right places */
				for(uint32_t i = 0; i < cols; i++)
					for(uint32_t j = 0; j < rows; j++)
						for(uint32_t k = 0; k < field_names.size(); k++)
							field_data[(i*nrows+j)*field_names.size()+k] = temp[(i*rows+j)*field_names.size()+k];
				rows = nrows;
				cols = ncols;
			}

		} else {
			field_data.reserve(nrows * ncols * field_names.size());
			for(uint32_t i = 0; i < nrows * ncols * field_names.size(); i++)
				field_data.push_back(NULL);
			rows = nrows;
			cols = ncols;
		}
	}
}

void MatStruct::add_field(std::string name) {
	bool present = false;
	for(uint32_t i = 0; i < field_names.size(); i++) {
		if(field_names[i]==name) {
			present = true;
			break;
		}
	}

	if(!present) {
		if(field_data.size()!=0) {
			/* We have alread some data into the struct, so we need to rearrange all the pointers! */

			/* First save the original vector into a temporary one */
			std::vector<MatMatrix*> temp = field_data;

			/* Second extend the original vector to the new size */
			field_data.reserve(rows * cols * (field_names.size()+1));
			for(uint32_t i = 0; i < rows*cols*field_names.size(); i++)
				field_data[i] = NULL;
			for(uint32_t i = rows*cols*field_names.size(); i < rows*cols*(field_names.size()+1); i++)
				field_data.push_back(NULL);

			/* Third copy the original data into the right places */
			for(uint32_t i = 0; i < cols; i++)
				for(uint32_t j = 0; j < rows; j++)
					for(uint32_t k = 0; k < field_names.size(); k++)
						field_data[(i*rows+j)*(field_names.size()+1)+k] = temp[(i*rows+j)*field_names.size()+k];

			/* Now we can add the new field */
			field_names.push_back(name);

		} else {
			field_names.push_back(name);
		}
	}
}

void MatStruct::add_field(const char * name) {
	this->add_field(std::string(name));
}

MatMatrix *&MatStruct::get_element(std::string field, uint32_t i, uint32_t j) {
	bool present = false;
	uint32_t field_num = 0;
	for(uint32_t k = 0; k < field_names.size(); k++) {
		if(field_names[k]==field) {
			present = true;
			field_num = k;
			break;
		}
	}

	if(!present) {
		field_num = field_names.size();
		add_field(field);

		if(i < rows && j < cols) {
			return field_data[(i*rows+j)*field_names.size()+field_num];

		} else {
			resize((i+1 > rows) ? i+1 : rows, (j+1 > cols) ? j+1 : cols);
			return field_data[(i*rows+j)*field_names.size()+field_num];
		}
	} else {
		if(i < rows && j < cols) {
			return field_data[(i*rows+j)*field_names.size()+field_num];

		} else {
			resize((i+1 > rows) ? i+1 : rows, (j+1 > cols) ? j+1 : cols);
			return field_data[(i*rows+j)*field_names.size()+field_num];
		}
	}
}

int MatStruct::write(MatFile &file) {
	if(file.IsOpen()) {
		uint32_t buffer[2];

		/* First we make the base header */
		uint8_t *base_header;
		uint32_t base_size;
		make_baseheader(base_header, base_size);

		/* Second we write the miMatrix header and we save the file pointer. */

		buffer[0] = miMATRIX;
		buffer[1] = 0;
		file.write((char *)buffer, 2*sizeof(uint32_t));
		int32_t start_pos = file.get_pos();

		/* Third we write the struct header */
		file.write((char *)base_header, base_size);

		/* We write the field name length element */
		buffer[0] = miINT32 | (4 << 16);
		buffer[1] = 0;
		for(uint32_t i = 0; i < field_names.size(); i++)
			if(field_names[i].length() > buffer[1])
				buffer[1] = field_names[i].length();
		buffer[1] += 1;
		file.write((char *)buffer, 2*sizeof(uint32_t));

		/* Then we write the field names */
		uint32_t names_size = buffer[1]*field_names.size();
		names_size += (names_size % 8) ? 8 - names_size % 8 : 0;

		char *names = new char[names_size];
		memset(names, 0x00, names_size);
		for(uint32_t i = 0; i < field_names.size(); i++)
			strcpy(names+(i*buffer[1]), field_names[i].c_str());

		buffer[0] = miINT8;
		buffer[1] = buffer[1]*field_names.size();
		file.write((char *)buffer, 2*sizeof(uint32_t));
		file.write((char *)names, names_size);

		/* Then we cycle over all elements */
		for(uint32_t i = 0; i < field_data.size(); i++) {
			if(field_data[i]) {
				if(field_data[i]->get_class_name() == std::string(typeid(double).name())) {
					MatVector<double> *matrix = dynamic_cast<MatVector<double>*>(field_data[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(field_data[i]->get_class_name() == std::string(typeid(float).name())) {
					MatVector<float> *matrix = dynamic_cast<MatVector<float>*>(field_data[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(field_data[i]->get_class_name() == std::string(typeid(uint32_t).name())) {
					MatVector<uint32_t> *matrix = dynamic_cast<MatVector<uint32_t>*>(field_data[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(field_data[i]->get_class_name() == std::string(typeid(int32_t).name())) {
					MatVector<int32_t> *matrix = dynamic_cast<MatVector<int32_t>*>(field_data[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(field_data[i]->get_class_name() == std::string(typeid(uint16_t).name())) {
					MatVector<uint16_t> *matrix = dynamic_cast<MatVector<uint16_t>*>(field_data[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(field_data[i]->get_class_name() == std::string(typeid(int16_t).name())) {
					MatVector<int16_t> *matrix = dynamic_cast<MatVector<int16_t>*>(field_data[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(field_data[i]->get_class_name() == std::string(typeid(uint8_t).name())) {
					MatVector<uint8_t> *matrix = dynamic_cast<MatVector<uint8_t>*>(field_data[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(field_data[i]->get_class_name() == std::string(typeid(int8_t).name())) {
					MatVector<int8_t> *matrix = dynamic_cast<MatVector<int8_t>*>(field_data[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(field_data[i]->get_class_name() == std::string("cell")) {
					MatCellArray *matrix = dynamic_cast<MatCellArray*>(field_data[i]);
					matrix->set_name("");
					matrix->write(file);

				} else if(field_data[i]->get_class_name() == std::string("struct")) {
					MatStruct *matrix = dynamic_cast<MatStruct*>(field_data[i]);
					matrix->set_name("");
					matrix->write(file);

				} else {
					std::cerr << "Runtime error! MatCellArray::write requested an unexpected type (" << field_data[i]->get_class_name() << ")." << std::endl;
					return -1;
				}

			} else {
				// We should write and empty array
				MatVector<double> matrix;
				matrix.set_class(mxDOUBLE_CLASS);
				matrix.write(file);
			}
		}

		/* In the end we get the data size */
		int32_t stop_pos = file.get_pos();
		buffer[1] = stop_pos - start_pos;

		file.set_pos(start_pos - 4);
		file.write((char *)(buffer+1), 4);

		file.set_pos(stop_pos);

		return 0;

	} else {
		return -1;
	}
}

int MatObj::add_obj(MatMatrix *obj) {
	uint8_t *header;
	uint32_t headsize, padding;

	if(obj->get_class_name() == std::string(typeid(double).name())) {
		MatVector<double> *matrix = dynamic_cast<MatVector<double>*>(obj);
		matrix->_priv_write(header, headsize, padding);
		this->headers.push_back(header);
		this->headsizes.push_back(headsize);
		this->dataptrs.push_back((uint8_t*)matrix->data);
		this->datasizes.push_back(matrix->datasize());
		this->paddings.push_back(padding);

	} else if(obj->get_class_name() == std::string(typeid(float).name())) {
		MatVector<float> *matrix = dynamic_cast<MatVector<float>*>(obj);
		matrix->_priv_write(header, headsize, padding);
		this->headers.push_back(header);
		this->headsizes.push_back(headsize);
		this->dataptrs.push_back((uint8_t*)matrix->data);
		this->datasizes.push_back(matrix->datasize());
		this->paddings.push_back(padding);

	} else if(obj->get_class_name() == std::string(typeid(uint32_t).name())) {
		MatVector<uint32_t> *matrix = dynamic_cast<MatVector<uint32_t>*>(obj);
		matrix->_priv_write(header, headsize, padding);
		this->headers.push_back(header);
		this->headsizes.push_back(headsize);
		this->dataptrs.push_back((uint8_t*)matrix->data);
		this->datasizes.push_back(matrix->datasize());
		this->paddings.push_back(padding);

	} else if(obj->get_class_name() == std::string(typeid(int32_t).name())) {
		MatVector<int32_t> *matrix = dynamic_cast<MatVector<int32_t>*>(obj);
		matrix->_priv_write(header, headsize, padding);
		this->headers.push_back(header);
		this->headsizes.push_back(headsize);
		this->dataptrs.push_back((uint8_t*)matrix->data);
		this->datasizes.push_back(matrix->datasize());
		this->paddings.push_back(padding);

	} else if(obj->get_class_name() == std::string(typeid(uint16_t).name())) {
		MatVector<uint16_t> *matrix = dynamic_cast<MatVector<uint16_t>*>(obj);
		matrix->_priv_write(header, headsize, padding);
		this->headers.push_back(header);
		this->headsizes.push_back(headsize);
		this->dataptrs.push_back((uint8_t*)matrix->data);
		this->datasizes.push_back(matrix->datasize());
		this->paddings.push_back(padding);

	} else if(obj->get_class_name() == std::string(typeid(int16_t).name())) {
		MatVector<int16_t> *matrix = dynamic_cast<MatVector<int16_t>*>(obj);
		matrix->_priv_write(header, headsize, padding);
		this->headers.push_back(header);
		this->headsizes.push_back(headsize);
		this->dataptrs.push_back((uint8_t*)matrix->data);
		this->datasizes.push_back(matrix->datasize());
		this->paddings.push_back(padding);

	} else if(obj->get_class_name() == std::string(typeid(uint8_t).name())) {
		MatVector<uint8_t> *matrix = dynamic_cast<MatVector<uint8_t>*>(obj);
		matrix->_priv_write(header, headsize, padding);
		this->headers.push_back(header);
		this->headsizes.push_back(headsize);
		this->dataptrs.push_back((uint8_t*)matrix->data);
		this->datasizes.push_back(matrix->datasize());
		this->paddings.push_back(padding);

	} else if(obj->get_class_name() == std::string(typeid(int8_t).name())) {
		MatVector<int8_t> *matrix = dynamic_cast<MatVector<int8_t>*>(obj);
		matrix->_priv_write(header, headsize, padding);
		this->headers.push_back(header);
		this->headsizes.push_back(headsize);
		this->dataptrs.push_back((uint8_t*)matrix->data);
		this->datasizes.push_back(matrix->datasize());
		this->paddings.push_back(padding);

	} else if(obj->get_class_name() == std::string("cell")) {
		//MatCellArray *matrix = dynamic_cast<MatCellArray*>(obj);
		// TODO

	} else if(obj->get_class_name() == std::string("struct")) {
		//MatStruct *matrix = dynamic_cast<MatStruct*>(obj);
		// TODO

	} else {
		std::cerr << "Runtime error! MatObj::add_obj requested an unexpected type (" << obj->get_class_name() << ")." << std::endl;
		return -1;
	}
	return 0;
}

size_t MatObj::get_bytes(uint8_t* buffer, size_t nb) {

	uint8_t* start = NULL;
	size_t max_write = 0;

	if(this->ptr >= this->total_size())
		return 0;

	if(ptr < 128) {
		// If ptr is less than 128 we have to send the file header
		start = (uint8_t*)mbuffer+ptr;
		max_write = (128-ptr > nb) ? nb : 128-ptr;

	} else {
		// Otherwise find actual object
		size_t i;
		size_t sz = 128;
		for(i = 0; i < headers.size(); i++) {
			size_t lsz = headsizes[i] + datasizes[i] + paddings[i];
			if(ptr < sz + lsz)
				break;
			else
				sz += lsz;
		}

		// Now i is the index of the current object and ptr-sz is the offset inside it
		// Check if we are still in the header
		if(ptr-sz < headsizes[i]) {
			// We are still in the header
			size_t written = ptr-sz;
			start = headers[i]+written;
			max_write = (headsizes[i]-written > nb) ? nb : headsizes[i]-written;

		} else if(ptr-sz-headsizes[i] < datasizes[i]) {
			// We are still in the header
			size_t written = ptr-sz-headsizes[i];
			start = dataptrs[i]+written;
			max_write = (datasizes[i]-written > nb) ? nb : datasizes[i]-written;

		} else {
			// We are in the padding area
			size_t written = ptr-sz-headsizes[i]-datasizes[i];
			max_write = (paddings[i]-written > nb) ? nb : paddings[i]-written;
		}
	}

	// Update ptr
	ptr += max_write;
	if(start != NULL) {
		memcpy((void*)buffer, (void*)start, max_write);
	} else {
		memset((void*)buffer, 0x00, max_write);
	}

	return max_write;
}
