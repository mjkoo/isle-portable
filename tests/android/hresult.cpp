#include "miniwin/d3drm.h"

#include <cassert>
#include <type_traits>

static_assert(sizeof(HRESULT) == 4);
static_assert(std::is_signed<HRESULT>::value);

int main()
{
	assert(SUCCEEDED(S_OK));
	assert(SUCCEEDED((HRESULT) 1));
	assert(!SUCCEEDED(DDERR_GENERIC));
	assert(!SUCCEEDED(E_NOINTERFACE));
	assert(!SUCCEEDED(DDERR_INVALIDPARAMS));
	assert(!SUCCEEDED(MAKE_DDHRESULT(785)));
}
