# Third-party notices

Parts of lilpet that come from others, and what ships with each build. This file goes with every firmware binary (the
BSD licences below ask for it).

| Part                                          | Where                                                             | Licence      |
| --------------------------------------------- | ----------------------------------------------------------------- | ------------ |
| Bosch BMI270 configuration file               | `firmware/main/bmi270_config.c`, in the firmware                  | BSD-3-Clause |
| LovyanGFX panel init sequences                | `firmware/main/display_atoms3r.c`, through M5GFX, in the firmware | FreeBSD      |
| M5GFX board values (pins, offsets, backlight) | `firmware/main/display_atoms3r.c`, in the firmware                | MIT          |
| ESP-IDF                                       | linked into the firmware                                          | Apache-2.0   |
| Waveshare demo (pins, CO5300 init)            | `firmware/main/display_co5300.c`, values read from the demo       | none stated  |
| Bricolage Grotesque font                      | in the PC app build, through `@fontsource-variable`               | OFL-1.1      |
| React and React DOM                           | in the PC app build                                               | MIT          |
| Emscripten runtime                            | in the WebAssembly build                                          | MIT          |

ESP-IDF: <https://github.com/espressif/esp-idf/blob/master/LICENSE>. Bricolage Grotesque: Copyright 2022 The Bricolage
Grotesque Project Authors, <https://github.com/ateliertriay/bricolage>, under the
[SIL Open Font License 1.1](https://openfontlicense.org/). React: <https://github.com/facebook/react/blob/main/LICENSE>.
Emscripten: <https://github.com/emscripten-core/emscripten/blob/main/LICENSE>.

## Bosch BMI270 configuration file

```text
* Copyright (c) 2023 Bosch Sensortec GmbH. All rights reserved.
 *
 * BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Configuration file of the BMI270, from bmi270.c in Bosch Sensortec's BMI270_SensorAPI
 * (https://github.com/boschsensortec/BMI270_SensorAPI): its bytes are unchanged, the code around them is reformatted.
 *
 * @file       bmi270.c
 * @date       2023-05-03
 * @version    v2.86.1
 *
```

## LovyanGFX

The panel classes of M5GFX (`Panel_ST7735.hpp`, `Panel_GC9A01.hpp`) come from LovyanGFX. Its
[licence file](https://github.com/lovyan03/LovyanGFX/blob/master/license.txt) also carries the notices of the libraries
it grew from.

```text
Software License Agreement (FreeBSD License)

Copyright (c) 2020 lovyan03 (https://github.com/lovyan03)

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.
```

## M5GFX

```text
MIT License

Copyright (c) 2021 M5Stack

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
