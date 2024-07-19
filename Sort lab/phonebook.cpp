#include "phonebook.h"
//#include "quicksort.h"

phonebook::phonebook(std::string &_surname, std::string &_name, std::string &_patronym, int _number)
{
	name = _name;
	surname = _surname;
	patronym = _patronym;
	number = _number;
}

phonebook::phonebook()
{
	name = "";
	surname = "";
	patronym = "";
	number = -1;
}

std::ostream &operator<<(std::ostream &os, const phonebook &phonebook)
{
	os << phonebook.surname << " " << phonebook.name << " " << phonebook.patronym << " " << phonebook.number;
	return os;
}

std::istream &operator>>(std::istream &is, phonebook &phonebook)
{
	is >> phonebook.surname >> phonebook.name >> phonebook.patronym >> phonebook.number;
	return is;
}

bool phonebook::operator<(const phonebook &second) const
{
	if (surname < second.surname)
		return true;
	if (second.surname < surname)
		return false;
	if (name < second.name)
		return true;
	if (second.name < name)
		return false;
	if (patronym < second.patronym)
		return true;
	if (second.patronym < patronym)
		return false;
	return number < second.number;
}

bool phonebook::operator>(const phonebook &second) const
{
	return second < *this;
}

bool phonebook::operator<=(const phonebook &second) const
{
	return !(second < *this);
}

bool phonebook::operator>=(const phonebook &second) const
{
	return !(*this < second);
}
