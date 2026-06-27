// This file is part of EnvSearcher.
//
// EnvSearcher is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// EnvSearcher is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// EnvSearcher. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <wchar.h>

wchar_t* run_printf(wchar_t* key, wchar_t* value);
wchar_t* normal(wchar_t* key, wchar_t* value);
wchar_t* hex_encode(wchar_t* key, wchar_t* value);
wchar_t* simple_escape(wchar_t* key, wchar_t* value);
wchar_t* name_only(wchar_t* key, wchar_t* value);
