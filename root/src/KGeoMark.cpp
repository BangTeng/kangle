#include "KGeoMark.h"
iterator_ret free_geo_env(void *data, void *argv)
{
	free(data);
	return iterator_remove_continue;
}
