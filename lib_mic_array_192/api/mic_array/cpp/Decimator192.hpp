/**
 * @file Decimator192.hpp
 * @brief Two One Stage Decimators for 192kHz sample rate PDM support.
 * This design implements a decimation factor of 16 to achieve 192kHz sample rate
 * from the 3.072MHz PDM sample rate. As the PdmRx implementation captures the
 * incoming PDM stream in blocks of 32 samples, this decimator filters each
 * 32 sample block twice. Once at the beginning with a padding of 16 samples at
 * the end, and with a padding of 16 samples at the beginning. So each call for
 * OneStageDecimator192:ProcessBlock() will produce two output samples.
 *
 * @author Christoph Kiener
 * @copyright This derivative work is subject to XMOS Public Licence Version 1
 * This file is a derivative work based on XMOS lib_mic_array library.
 * Original work Copyright (c) XMOS Limited
 * Modifications for 192kHz support by Christoph Kiener
 */

#pragma once

#include <cstdint>
#include <string>
#include <cassert>

#include "xmath/filter.h"
#include "mic_array/etc/fir_1x16_bit.h"

#define S1_TAP_COUNT 256
#define S1_WORDS (S1_TAP_COUNT) / 2

// taps=240, fc=80kHz, window=("kaiser", 4.0), a_stop=-44dB, 16 samples padding at the end
// clang-format off
static const uint32_t WORD_ALIGNED s1_fir_zero_after[S1_WORDS] = {
  0xFFFFDA39, 0xBFF03D14, 0x538A5CDE, 0xCE092678, 0xAA551E64, 0x90737B3A, 0x51CA28BC, 0x0FFD9C5B, 
  0xFFFF0B0A, 0x66F123BA, 0x52CDEEBC, 0x9ABFF4AE, 0xF66F752F, 0xFD593D77, 0xB34A5DC4, 0x8F6650D0, 
  0xFFFFE5F6, 0x6942B926, 0xA4759759, 0x7664D0A0, 0xA815050B, 0x266E9AE9, 0xAE25649D, 0x42966FA7, 
  0xFFFF9207, 0xCF40DCF9, 0x3DBEE8B1, 0xBF02757E, 0xF00F7EAE, 0x40FD8D17, 0x7DBC9F3B, 0x02F3E049, 
  0xFFFFA150, 0xE96BC170, 0x45B01821, 0x3D7A8121, 0xEE778481, 0x5EBC8418, 0x0DA20E83, 0xD6970A85, 
  0xFFFF959A, 0x0626D835, 0x1E635D0D, 0x75D96DDB, 0xF24FDBB6, 0x9BAEB0BA, 0xC678AC1B, 0x646059A9, 
  0xFFFF8CB6, 0x0AE19A19, 0xBB279875, 0xCD6B6F6F, 0x8001F6F6, 0xD6B3AE19, 0xE4DD9859, 0x87506D31, 
  0xFFFF7C71, 0xF34AE6A1, 0xD79AB09E, 0x821667F1, 0xD42B8FE6, 0x6841790D, 0x59EB8567, 0x52CF8E3E, 
  0xFFFFFC0F, 0xFC730194, 0xB0298AF7, 0xAAEDFBAA, 0x7E7E55DF, 0xB755EF51, 0x940D2980, 0xCE3FF03F, 
  0xFFFFFC00, 0x007C0073, 0x8FCD2CF2, 0xCCA10833, 0xDC3BCC10, 0x85334F34, 0xB3F1CE00, 0x3E00003F, 
  0xFFFFFC00, 0x007FFFF0, 0x7FF1CF0E, 0x5A61A7C3, 0xC813C3E5, 0x865A70F3, 0x8FFE0FFF, 0xFE00003F, 
  0xFFFFFC00, 0x007FFFF0, 0x0001F001, 0xC61E3556, 0x90096AAC, 0x7863800F, 0x80000FFF, 0xFE00003F, 
  0xFFFFFC00, 0x007FFFF0, 0x0001FFFF, 0xC1FFC664, 0xE0072663, 0xFF83FFFF, 0x80000FFF, 0xFE00003F, 
  0xFFFFFC00, 0x007FFFF0, 0x0001FFFF, 0xC0000787, 0x0000E1E0, 0x0003FFFF, 0x80000FFF, 0xFE00003F, 
  0xFFFFFC00, 0x007FFFF0, 0x0001FFFF, 0xC00007F8, 0x00001FE0, 0x0003FFFF, 0x80000FFF, 0xFE00003F, 
  0x000003FF, 0xFF80000F, 0xFFFE0000, 0x3FFFF800, 0x0000001F, 0xFFFC0000, 0x7FFFF000, 0x01FFFFC0,
};
// clang-format on

// taps=240, fc=80kHz, window=("kaiser", 4.0), a_stop=-44dB, 16 samples padding at the beginning
// clang-format off
static const uint32_t WORD_ALIGNED s1_fir_zero_before[S1_WORDS] = {
  0xDA39BFF0, 0x3D14538A, 0x5CDECE09, 0x2678AA55, 0x1E649073, 0x7B3A51CA, 0x28BC0FFD, 0x9C5BFFFF, 
  0x0B0A66F1, 0x23BA52CD, 0xEEBC9ABF, 0xF4AEF66F, 0x752FFD59, 0x3D77B34A, 0x5DC48F66, 0x50D0FFFF, 
  0xE5F66942, 0xB926A475, 0x97597664, 0xD0A0A815, 0x050B266E, 0x9AE9AE25, 0x649D4296, 0x6FA7FFFF, 
  0x9207CF40, 0xDCF93DBE, 0xE8B1BF02, 0x757EF00F, 0x7EAE40FD, 0x8D177DBC, 0x9F3B02F3, 0xE049FFFF, 
  0xA150E96B, 0xC17045B0, 0x18213D7A, 0x8121EE77, 0x84815EBC, 0x84180DA2, 0x0E83D697, 0x0A85FFFF, 
  0x959A0626, 0xD8351E63, 0x5D0D75D9, 0x6DDBF24F, 0xDBB69BAE, 0xB0BAC678, 0xAC1B6460, 0x59A9FFFF, 
  0x8CB60AE1, 0x9A19BB27, 0x9875CD6B, 0x6F6F8001, 0xF6F6D6B3, 0xAE19E4DD, 0x98598750, 0x6D31FFFF, 
  0x7C71F34A, 0xE6A1D79A, 0xB09E8216, 0x67F1D42B, 0x8FE66841, 0x790D59EB, 0x856752CF, 0x8E3EFFFF, 
  0xFC0FFC73, 0x0194B029, 0x8AF7AAED, 0xFBAA7E7E, 0x55DFB755, 0xEF51940D, 0x2980CE3F, 0xF03FFFFF, 
  0xFC00007C, 0x00738FCD, 0x2CF2CCA1, 0x0833DC3B, 0xCC108533, 0x4F34B3F1, 0xCE003E00, 0x003FFFFF, 
  0xFC00007F, 0xFFF07FF1, 0xCF0E5A61, 0xA7C3C813, 0xC3E5865A, 0x70F38FFE, 0x0FFFFE00, 0x003FFFFF, 
  0xFC00007F, 0xFFF00001, 0xF001C61E, 0x35569009, 0x6AAC7863, 0x800F8000, 0x0FFFFE00, 0x003FFFFF, 
  0xFC00007F, 0xFFF00001, 0xFFFFC1FF, 0xC664E007, 0x2663FF83, 0xFFFF8000, 0x0FFFFE00, 0x003FFFFF, 
  0xFC00007F, 0xFFF00001, 0xFFFFC000, 0x07870000, 0xE1E00003, 0xFFFF8000, 0x0FFFFE00, 0x003FFFFF, 
  0xFC00007F, 0xFFF00001, 0xFFFFC000, 0x07F80000, 0x1FE00003, 0xFFFF8000, 0x0FFFFE00, 0x003FFFFF, 
  0x03FFFF80, 0x000FFFFE, 0x00003FFF, 0xF8000000, 0x001FFFFC, 0x00007FFF, 0xF00001FF, 0xFFC00000, 
};
// clang-format on

namespace mic_array
{

  /**
   * @brief One Stage Decimator
   *
   * This class template represents a one stage decimator which converts a stream
   * of PDM samples to a 1/16th sample rate stream of PCM samples.
   *
   * Concrete implementations of this class template are meant to be used as the
   * `TDecimator` template parameter in the @ref MicArray class template.
   *
   * @tparam MIC_COUNT      Number of microphone channels.
   */
  template <unsigned MIC_COUNT>
  class OneStageDecimator192
  {

  public:
    /**
     * Number of microphone channels.
     */
    static constexpr unsigned MicCount = MIC_COUNT;

  private:
    /**
     * Stage 1 decimator configuration and state.
     */
    struct
    {
      /**
       * Pointer to alternating filter coefficients for Stage 1
       */
      const uint32_t *filter_coef[2];
      /**
       * Filter state (PDM history) for stage 1 filters.
       */
      uint32_t pdm_history[MIC_COUNT][8]
#ifndef __DOXYGEN__ // doxygen breaks if it encounters this.
                    // Must be initialized in this way. Initializing the history values in the
                    // constructor causes an XCore-specific problem. Specifically, if the
                    // MicArray instance where this decimator is used is declared outside of a
                    // function scope (that is, as a global- or module-scope object; which it
                    // ordinarily will be), initializing the PDM history within the
                    // constructor forces the compiler to allocate the object on _all tiles_.
                    // Being allocated on a tile where it is not used does not by itself break
                    // anything, but it does result in less memory being available for other
                    // things on that tile. Initializing the history in this way prevents
                    // that.
          = {[0 ...(MIC_COUNT - 1)] = {[0 ... 7] = 0x55555555}}
#endif
      ;
    } stage1;

  public:
    constexpr OneStageDecimator192() noexcept {}

    /**
     * @brief Initialize the decimator.
     *
     * Sets the stage 1 filter coefficients. The decimator must be
     * initialized before any calls to `ProcessBlock()`.
     *
     */
    void Init();

    /**
     * @brief Process one block of PDM data.
     *
     * Processes a block of PDM data to produce two output sample from the
     * first stage decimator.
     *
     * One `pdm_block` (32bit) contains enough PDM samples to produce two output
     * samples from the first stage decimator. (32 / 16 = 2)
     *
     * Two output samples from the first stage decimator are computed and
     * written to `sample_out[2][]`.
     *
     * @param sample_out  Output sample vector.
     * @param pdm_block   PDM data to be processed.
     */
    void ProcessBlock(
        int32_t sample_out[2][MIC_COUNT],
        uint32_t pdm_block[MIC_COUNT]);
  };

}

//////////////////////////////////////////////
// Template function implementations below. //
//////////////////////////////////////////////

template <unsigned MIC_COUNT>
void mic_array::OneStageDecimator192<MIC_COUNT>::Init()
{
  this->stage1.filter_coef[0] = s1_fir_zero_after;
  this->stage1.filter_coef[1] = s1_fir_zero_before;
}

template <unsigned MIC_COUNT>
void mic_array::OneStageDecimator192<MIC_COUNT>::ProcessBlock(
    int32_t sample_out[2][MIC_COUNT],
    uint32_t pdm_block[MIC_COUNT])
{
  for (size_t mic = 0; mic < MIC_COUNT; mic++)
  {
    uint32_t *hist = &this->stage1.pdm_history[mic][0];

    hist[0] = pdm_block[mic];
    sample_out[0][mic] = (fir_1x16_bit(hist, this->stage1.filter_coef[0]) << 3);
    sample_out[1][mic] = (fir_1x16_bit(hist, this->stage1.filter_coef[1]) << 3);
    shift_buffer(hist);
  }
}
