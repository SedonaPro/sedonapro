// -*- mode: c++ -*-
//
// Copyright 2016 Andrey Skvortsov <andrej.skvortzov@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "sedona.h"
#include <math.h>



Cell sproMath_Math_arccos(SedonaVM* vm, Cell* params)
{
	float v = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = acos(v);
	return ret_cell;
}


Cell sproMath_Math_arcsin(SedonaVM* vm, Cell* params)
{
	float v = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = asin(v);
	return ret_cell;
}

Cell sproMath_Math_arctan(SedonaVM* vm, Cell* params)
{
	float v = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = atan(v);
	return ret_cell;
}


Cell sproMath_Math_arctan2(SedonaVM* vm, Cell* params)
{
	float in1 = params[0].fval;
	float in2 = params[1].fval;
	Cell ret_cell;
	ret_cell.fval = atan2(in1, in2);
	return ret_cell;
}

Cell sproMath_Math_ceil(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = ceil(in);
	return ret_cell;
}

Cell sproMath_Math_cos(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = cos(in);
	return ret_cell;
}

Cell sproMath_Math_cosh(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = cosh(in);
	return ret_cell;
}

Cell sproMath_Math_exp(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = exp(in);
	return ret_cell;
}

Cell sproMath_Math_floor(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = floor(in);
	return ret_cell;
}

Cell sproMath_Math_fmod(SedonaVM* vm, Cell* params)
{
	float in1 = params[0].fval;
	float in2 = params[1].fval;
	Cell ret_cell;
	ret_cell.fval = fmod(in1, in2);
	return ret_cell;
}

Cell sproMath_Math_frexp(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	float *mantissa = params[0].aval;
	int *exp = params[1].aval;
	*mantissa = frexp(in, exp);
	return zeroCell;
}

Cell sproMath_Math_ldexp(SedonaVM* vm, Cell* params)
{
	float mantissa = params[0].fval;
	float exp = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = ldexp(mantissa, exp);
	return ret_cell;
}

Cell sproMath_Math_log(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = log(in);
	return ret_cell;
}

Cell sproMath_Math_log10(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = log10(in);
	return ret_cell;
}

Cell sproMath_Math_modf(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	float *intPart = params[1].aval;
	float *fracPart = params[2].aval;
	Cell ret_cell;
	*fracPart = modff(in, intPart);
	return zeroCell;
}

Cell sproMath_Math_pow(SedonaVM* vm, Cell* params)
{
	float base = params[0].fval;
	float exp = params[1].fval;
	Cell ret_cell;
	ret_cell.fval = pow(base, exp);
	return ret_cell;
}

Cell sproMath_Math_sin(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = sin(in);
	return ret_cell;
}

Cell sproMath_Math_sinh(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = sinh(in);
	return ret_cell;
}


Cell sproMath_Math_sqrt(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = sqrt(in);
	return ret_cell;
}

Cell sproMath_Math_tan(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = tan(in);
	return ret_cell;
}

Cell sproMath_Math_tanh(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = tanh(in);
	return ret_cell;
}


Cell sproMath_Math_cbrt(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = cbrt(in);
	return ret_cell;
}

Cell sproMath_Math_arccosh(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = acosh(in);
	return ret_cell;
}

Cell sproMath_Math_arcsinh(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = asinh(in);
	return ret_cell;
}

Cell sproMath_Math_arctanh(SedonaVM* vm, Cell* params)
{
	float in = params[0].fval;
	Cell ret_cell;
	ret_cell.fval = atanh(in);
	return ret_cell;
}
