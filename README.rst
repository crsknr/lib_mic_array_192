
:orphan:

###############################################################
lib_mic_array_192: Modified PDM microphone array library for 192kHz
###############################################################

:vendor: Modified from XMOS original
:version: 5.5.0-192khz-mod
:scope: General Use
:description: PDM microphone array library with 192kHz support
:category: General Purpose
:keywords: PDM, microphone, 192kHz
:devices: xcore.ai

*******
Summary
*******

This is a modified version of the XMOS microphone array library, extended to support 192kHz sample rates.
The original XMOS microphone array library is designed to allow interfacing to PDM microphones coupled with efficient decimation to user configurable output sample rates.

**Modifications by Christoph Kiener:**
  - Added 192kHz output sample rate support
  - Implemented OneStageDecimator192 class for 16x decimation (3.072 MHz PDM → 192 kHz)
  - Custom filter coefficients optimized for 192kHz operation

**Original XMOS Library:**
  - Based on lib_mic_array v5.5.0 from XMOS Limited.
  - Original repository: https://github.com/xmos/lib_mic_array

This library is only available for XS3 devices due to requiring the XS3 vector unit.

********
Features
********

The microphone array library has the following features:

**Original Features:**
  - 48, 32, 16 kHz output sample rates by default (3.072 MHz PDM clock)
  - 44.1, 29.4, 14.7 kHz output samples using 2.8224 MHz PDM clock
  - Other sample rates possible using custom decimation filter
  - 1 to 16 PDM microphones
  - Supports up to 8 microphones using only a single thread
  - Configurable MCLK to PDM clock divider
  - Supports both SDR and DDR microphone configurations
  - Framing with configurable frame size
  - DC offset removal
  - Extensible C++ design

**Additional 192kHz Features:**
  - 192 kHz output sample rate support (3.072 MHz PDM clock with 16x decimation)
  - OneStageDecimator192 implementation for optimized 192kHz processing (``lib_mic_array_192/api/mic_array/cpp/Decimator192.hpp``)
  - Custom Kaiser window filter coefficients (240 taps, fc=80kHz, a_stop=-44dB)
  - Add division parameter to ``mic_array_pdm_clock_start`` to allow for starting up MEMS mics with a lower PDM clock

************
Known issues
************

  * PDM receive can lock-up in ISR mode when ma_frame_rx is not called isochronously after first transfer.

Also see https://github.com/xmos/lib_mic_array/issues.

****************
Development repo
****************

**Modified Version:**
  * This repository (lib_mic_array_192)

**Original XMOS Repository:**  
  * ``lib_mic_array <https://www.github.com/xmos/lib_mic_array>``

******************
Attribution & License
******************

This work is based on lib_mic_array v5.5.0 by XMOS Limited and is subject to the XMOS Public Licence Version 1.
The original work is copyright XMOS Limited. Modifications for 192kHz support by Christoph Kiener.

All modifications comply with Section 2.2 of the XMOS Public Licence regarding derivatives.

**************
Required tools
**************

  * XMOS XTC Tools: 15.3.0

*********************************
Required libraries (dependencies)
*********************************

  * `lib_xcore_math <https://www.xmos.com/file/lib_xcore_math>`_

*************************
Related application notes
*************************

The following application notes use this library:

  * AN000248 - Using lib_xua with lib_mic_array

*******
Support
*******

This package is supported by XMOS Ltd. Issues can be raised against the software at: http://www.xmos.com/support
