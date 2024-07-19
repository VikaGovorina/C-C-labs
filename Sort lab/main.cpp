#include "phonebook.h"
#include "quicksort.h"
#include "return_codes.h"

#include <cstdlib>
#include <iostream>

using namespace std;

template< typename T, bool descending >
size_t qs(FILE* out, int size)
{
	T* res = new T[size];
	if (res == nullptr)
	{
		cerr << "cannot allocate memory";
		return ERROR_NOT_ENOUGH_MEMORY;
	}
	for (int i = 0; i < size; i++)
	{
		cin >> res[i];
	}
	int l = 0;
	quicksort< T, descending >(res, l, size);
	for (size_t i = 0; i < size; i++)
	{
		cout << res[i] << "\n";
	}
	delete[](res);
	fclose(out);
	return ERROR_SUCCESS;
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		cerr << "wrong number of arguments\n";
		return ERROR_INVALID_DATA;
	}
	FILE* in = freopen(argv[1], "r", stdin);
	if (in == nullptr)
	{
		cerr << "cannot open an input file\n";
		return ERROR_FILE_NOT_FOUND;
	}
	FILE* out = freopen(argv[2], "w", stdout);
	if (out == nullptr)
	{
		cerr << "cannot open an output file\n";
		return ERROR_FILE_NOT_FOUND;
	}
	string type, mode;
	cin >> type >> mode;

	int size;
	cin >> size;

	if (mode == "descending")
	{
		if (type == "int")
		{
			qs< int, true >(out, size);
		}
		else if (type == "float")
		{
			qs< float, true >(out, size);
		}
		else if (type == "phonebook")
		{
			qs< phonebook, true >(out, size);
		}
	}
	else if (mode == "ascending")
	{
		if (type == "int")
		{
			qs< int, false >(out, size);
		}
		else if (type == "float")
		{
			qs< float, false >(out, size);
		}
		else if (type == "phonebook")
		{
			qs< phonebook, false >(out, size);
		}
	}
	else
	{
		cerr << "unknown mode";
		return ERROR_NOT_IMPLEMENTED;
	}

	fclose(in);

	return 0;
}
