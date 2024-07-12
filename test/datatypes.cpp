/*
 * This file is a part of mfmidi.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "mfmidi/midi_tempo.hpp"
#include "mfmidi/smf/division.hpp"

using namespace mfmidi;

static_assert(sizeof(tempo) == 4);

static_assert(sizeof(division) == 2);
static_assert(static_cast<division>(0xE250).fps() == 30);
static_assert(static_cast<division>(0xE250).tpf() == 80);

int main()
{
}
