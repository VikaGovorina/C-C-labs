template< typename T, bool descending >
void swap_fun(T &a, T &b)
{
	T tmp = a;
	a = b;
	b = tmp;
}

template< typename T, bool descending >
int partition(T *a, int l, int r)
{
	T x = a[l];
	int i = l;
	for (int j = l + 1; j < r; j++)
	{
		if (!descending)
		{
			if (a[j] <= x)
			{
				i++;
				swap_fun< T, descending >(a[i], a[j]);
			}
		}
		else
		{
			if (x <= a[j])
			{
				i++;
				swap_fun< T, descending >(a[i], a[j]);
			}
		}
	}
	swap_fun< T, descending >(a[i], a[l]);
	return i;
}

template< typename T, bool descending >
void quicksort(T *a, int l, int r)
{
	int mid;
	if (l < r)
	{
		mid = partition< T, descending >(a, l, r);
		quicksort< T, descending >(a, l, mid);
		quicksort< T, descending >(a, mid + 1, r);
	}
}
