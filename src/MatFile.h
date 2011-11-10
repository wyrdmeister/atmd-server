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

/*  C++ MAT v5.0 library version 1.0
 *
 *  This file must be used along with MatFile.cpp
 * 
 *  MatFile.h and MatFile.ccp implements a simple C++ class to write Matlab .mat files
 *  using the format from Matlab 5.0
 *
 *  Changelog:
 *  1.0 - Release (Aug-2011) Michele Devetta
 */

#ifndef __MatFile_h__
#define __MatFile_h__

#include <string.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <typeinfo>
#include <stdint.h>

/* Mat header */
#define MAT_HEADER "MATLAB 5.0 MAT-file, Platform: PCWIN, Created on: "
#define MAT_VERSION_1B 0x00
#define MAT_VERSION_2B 0x01
#define MAT_ENDIAN 0x4D49

/* Data types */
#define miINT8    1 /* 8 bit signed */
#define miUINT8   2 /* 8 bit signed */
#define miINT16   3 /* 16 bit signed */
#define miUINT16  4 /* 16 bit signed */
#define miINT32   5 /* 32 bit signed */
#define miUINT32  6 /* 32 bit signed */
#define miSINGLE  7 /* IEEE 754 single format */
#define miDOUBLE  9 /* IEEE 754 double format */
#define miINT64  12 /* 64 bit signed */
#define miUINT64 13 /* 64 bit signed */
#define miMATRIX 14 /* Matlab matrix */

/* Array classes */
#define mxCELL_CLASS    1 /* Cell array */
#define mxSTRUCT_CLASS  2 /* Structure */
#define mxOBJECT_CLASS  3 /* Object */
#define mxCHAR_CLASS    4 /* Character array */
#define mxSPARSE_CLASS  5 /* Sparse array */
#define mxDOUBLE_CLASS  6 /* Double precision array */
#define mxSINGLE_CLASS  7 /* Single precision array */
#define mxINT8_CLASS    8 /* 8-bit, signed integer */
#define mxUINT8_CLASS   9 /* 8-bit, unsigned integer */
#define mxINT16_CLASS  10 /* 16-bit, signed integer */
#define mxUINT16_CLASS 11 /* 16-bit, unsigned integer */
#define mxINT32_CLASS  12 /* 32-bit, signed integer */
#define mxUINT32_CLASS 13 /* 32-bit unsigned, integer */


/*
 * CLASS: MatFile - This class is used to write objects to a .mat file (Matlab v6 format)
 */
class MatFile {
public:
	MatFile() {};
	MatFile(const char * filename) { open(filename); }
	MatFile(std::string &filename) { open(filename); }
	~MatFile() { if(file.is_open()) file.close(); }

	void open(std::string &filename);
	void open(const char * filename);
	bool IsOpen() { return file.is_open(); }
	void close() { if(file.is_open()) file.close(); }
	int write(char * buffer, uint32_t size) {
		if(file.is_open()) {
			file.write(buffer, size);
			return 0;
		} else {
			return -1;
		}
	}
	int32_t get_pos() { return file.tellg(); }
	void set_pos(int32_t pos) { file.seekg(pos, std::ios::beg); }

private:
	std::fstream file;
};


/*
 * CLASS: MatMatrix - This is the parent class for all objects we will write into the mat file.
 */
class MatMatrix {
public:
	MatMatrix() : size(0), flags(0), mat_class(0), rows(0), cols(0), class_name(std::string("")) {}
	virtual ~MatMatrix() {}

	void set_class(uint8_t cl) { mat_class = cl; }
	uint8_t get_class() { return mat_class; }

	void set_logical(bool en) { flags = (flags & 0x0C) | (((en) ? 0x1 : 0x0) << 1); }
	void set_global(bool en) { flags = (flags & 0x0A) | (((en) ? 0x1 : 0x0) << 2); }
	void set_complex(bool en) { flags = (flags & 0x06) | (((en) ? 0x1 : 0x0) << 3); }
	bool get_logical() { return ((flags & 0x02)!=0); }
	bool get_global() { return ((flags & 0x04)!=0); }
	bool get_complex() { return ((flags & 0x08)!=0); }

	void set_name(std::string name) { array_name = name; }
	void set_name(const char * name) { array_name = std::string(name); }
	std::string &get_name() { return array_name; }
	std::string &get_class_name() { return class_name; }

	uint32_t header_size();
	void make_baseheader(uint8_t *&buffer, uint32_t &size);

protected:
	uint32_t size;
	uint8_t flags;
	uint8_t mat_class;

	uint32_t rows;
	uint32_t cols;
	std::string array_name;
	std::string class_name;
};


/*
 * CLASS: MatVector<T> - This class represent a matrix of type T
 */
template <class T> class MatVector : public MatMatrix {
public:
	MatVector(): data(NULL) { class_name = std::string(typeid(T).name()); }
	MatVector(const char * name, uint8_t class_id): data(NULL) {
		set_name(name);
		set_class(class_id);
		class_name = std::string(typeid(T).name());
	}
	MatVector(MatVector &mv) {}
	~MatVector() { if(data) delete(data); }

	void resize(uint32_t nrows, uint32_t ncols);
	void clear() { if(data) delete(data); data = NULL; rows = 0; cols = 0; }
	void zero() { if(data) for(uint32_t i = 0; i < rows*cols; i++) data[i] = 0.0; }

	T &operator()(uint32_t i, uint32_t j);

	int write(MatFile &file);

	size_t datasize() { return (cols * rows * sizeof(T)); };

protected:
	int _priv_write(uint8_t *&header, uint32_t &headsize, uint32_t &padding);

private:
	T *data; // Pointer to the data array

friend class MatObj;
};


/*
 * CLASS: MatVector<T> - Members definition
 */
template <class T> T &MatVector<T>::operator()(uint32_t i, uint32_t j) {
	if(i < rows && j < cols) {
		return data[i+rows*j];

	} else {
		resize((i+1 > rows) ? i+1 : rows, (j+1 > cols) ? j+1 : cols);
		return data[i+rows*j];
	}
}

template <class T> void MatVector<T>::resize(uint32_t nrows, uint32_t ncols) {
	if(data) {
		if(nrows < rows)
			nrows = rows;
		if(ncols < cols)
			nrows = cols;
		if(nrows > rows || ncols > cols) {
			/* Allocate new data array */
			T *new_data = new T[nrows*ncols];
			memset((char *)new_data, 0x00, nrows * ncols * sizeof(T));

			/* Copy data to new array */
			for(uint32_t i = 0; i < cols; i++)
				for(uint32_t j = 0; j < rows; j++)
					new_data[nrows*i+j] = data[rows*i+j];

			/* Free old array */
			if(data)
				delete data;

			/* Update structure */
			data = new_data;
			rows = nrows;
			cols = ncols;
		}
	} else {
		data = new T[nrows*ncols];
		memset((char *)data, 0x00, nrows * ncols * sizeof(T));
		rows = nrows;
		cols = ncols;
	}
}

template <class T> int MatVector<T>::_priv_write(uint8_t *&header, uint32_t &headsize, uint32_t &padding) {

	// First we check that the data type is known
	uint32_t type = 0;
	if(typeid(T) == typeid(double))
		type = miDOUBLE;
	else if(typeid(T) == typeid(float))
		type = miSINGLE;
	else if(typeid(T) == typeid(uint32_t))
		type = miUINT32;
	else if(typeid(T) == typeid(int32_t))
		type = miINT32;
	else if(typeid(T) == typeid(uint16_t))
		type = miUINT16;
	else if(typeid(T) == typeid(int16_t))
		type = miINT16;
	else if(typeid(T) == typeid(uint8_t))
		type = miUINT8;
	else if(typeid(T) == typeid(int8_t))
		type = miINT8;
	else
		std::cerr << "Runtime error! MatVector<T>::get_header requested an unexpected type (" << typeid(T).name() << ")." << std::endl;
	if(type == 0)
		return -1;

	// second we make the base header
	uint8_t *base_header;
	uint32_t base_size;
	make_baseheader(base_header, base_size);

	// Third we compute the total header size and allocate a suitable buffer
	headsize = base_size + 4*sizeof(uint32_t);
	header = new uint8_t[headsize];
	if(header == NULL)
		return -1;

	// Fourth we check if we need extra padding at the end of the data section
	padding = (datasize() % 8 != 0) ? 8 - datasize() % 8 : 0;

	// Fifth we write the miMatrix header
	*((uint32_t*)header) = miMATRIX;
	*((uint32_t*)(header+4)) = base_size + 8 + datasize() + padding;

	// Sixth we write the base header and the data header
	memcpy((void*)(header+8), (void*)base_header, base_size);
	delete base_header;
	*((uint32_t*)(header+8+base_size)) = type;
	*((uint32_t*)(header+8+base_size+4)) = datasize();

	return 0;
}

template <class T> int MatVector<T>::write(MatFile &file) {
	if(file.IsOpen()) {

		uint8_t *header;
		uint32_t headsize, padding;

		// Compute headers
		_priv_write(header, headsize, padding);

		// Write to file
		file.write((char *)header, headsize);
		delete header;

		file.write((char *)data, datasize());

		/* Last we write the padding */
		int8_t pad = 0;
		for(uint32_t i = 0; i < padding; i++)
			file.write((char *)&pad, 1);

		return 0;

	} else {
		return -1;
	}
}


/*
 * CLASS: MatCellArray
 */
class MatCellArray : public MatMatrix {
public:
	MatCellArray() { mat_class = mxCELL_CLASS; class_name = "cell"; }
	MatCellArray(MatCellArray &mca) {}
	~MatCellArray() {}

	void set_class(uint8_t cl) {} // Class cannot be changed in a cell array
	void resize(uint32_t nrows, uint32_t ncols);

	MatMatrix *&operator()(uint32_t i, uint32_t j);

	int write(MatFile &file);

private:
	std::vector<MatMatrix*> cells;

friend class MatObj;
};


/*
 * CLASS: MatStruct
 */
class MatStruct : public MatMatrix {
public:
	MatStruct() { mat_class = mxSTRUCT_CLASS; class_name = "struct"; }
	MatStruct(MatStruct &ms) {}
	~MatStruct() {}

	void set_class(uint8_t cl) {} // Class cannot be changed in a struct
	void resize(uint32_t nrows, uint32_t ncols);

	void add_field(std::string name);
	void add_field(const char * name);

	MatMatrix *&operator()(std::string field, uint32_t i, uint32_t j) { return get_element(field, i, j); }
	MatMatrix *&operator()(const char * field, uint32_t i, uint32_t j)  { return get_element(std::string(field), i, j); }

	int write(MatFile &file);

private:
	MatMatrix *&get_element(std::string field, uint32_t i, uint32_t j);

	std::vector<std::string> field_names;
	std::vector<MatMatrix*> field_data;

friend class MatObj;
};


/* Class MAT object (for raw data writing) */
class MatObj {
public:
	MatObj() : ptr(0) {
		/* Format the timestamp */
		time_t current_time = time(NULL);
		char *date_str = ctime(&current_time);

		/* Fill header */
		memset(mbuffer, 0x20, 128);
		memcpy(mbuffer, MAT_HEADER, strlen(MAT_HEADER));
		memcpy(mbuffer+strlen(MAT_HEADER), date_str, strlen(date_str));
		*(uint8_t*)(mbuffer+124) = MAT_VERSION_1B;
		*(uint8_t*)(mbuffer+125) = MAT_VERSION_2B;
		*(uint16_t*)(mbuffer+126) = MAT_ENDIAN;
	};
	~MatObj () {
		for(size_t i = 0; i < headers.size(); i++)
			delete headers[i];
	};

	int add_obj(MatMatrix *obj);

	size_t get_bytes(uint8_t* buffer, size_t nb);

	size_t total_size() {
		size_t sz = 128;
		for(size_t i = 0; i < headers.size(); i++) {
			sz += headsizes[i];
			sz += datasizes[i];
			sz += paddings[i];
		}
		return sz;
	};

	void reset() { ptr = 0; };
	void clear() {
		ptr = 0;
		headers.clear();
		headsizes.clear();
		dataptrs.clear();
		datasizes.clear();
		paddings.clear();
	}

private:
	size_t ptr;
	std::vector<uint8_t*> headers;
	std::vector<uint32_t> headsizes;
	std::vector<uint8_t*> dataptrs;
	std::vector<uint32_t> datasizes;
	std::vector<uint32_t> paddings;
	char mbuffer[128];
};

#endif
