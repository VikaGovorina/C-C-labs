#include <cstdlib>
#include <fstream>
#include <iostream>
#include <utility>

struct phonebook
{
	std::string surname;
	std::string name;
	std::string patronym;
	int number;

	phonebook();

	phonebook(std::string &_surname, std::string &_name, std::string &_patronym, int _number);

	friend std::ostream &operator<<(std::ostream &os, const phonebook &phonebook);
	friend std::istream &operator>>(std::istream &is, phonebook &phonebook);

	bool operator<=(const phonebook &second) const;

	bool operator>=(const phonebook &second) const;
	bool operator<(const phonebook &second) const;
	bool operator>(const phonebook &second) const;
};
