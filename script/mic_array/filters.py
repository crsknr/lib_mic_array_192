# Copyright 2022 XMOS LIMITED.
# This Software is subject to the terms of the XMOS Public Licence: Version 1.

import numpy as np
import pickle as pkl
from . import util


def VLMACCR1(vC: np.ndarray, mem: np.ndarray, acc = 0, **kwargs) -> np.int32:
  # vC and mem should both be binary {0,1} vectors that are 256 elements
  assert vC.ndim == 1
  assert mem.ndim == 1
  assert len(vC) == 256
  assert len(mem) == 256

  q = np.int(1)*(~(vC == mem))
  popcount_xor = np.sum(q)
  return acc + (128 - popcount_xor)



class Stage1Filter(object):

  INT16_MAX_COEFFICIENT = 32766
  BLOCK_SIZE = 256

  def __init__(self, coefs: np.ndarray, decimation_factor: int = 32):
    assert (coefs.ndim == 1), "Stage1Filter coefs must be a single dimensional ndarray"
    assert (len(coefs) % Stage1Filter.BLOCK_SIZE == 0), f"Stage1Filter must have a multiple of 256 coefficients ({len(coefs)})"

    t = np.argmax(np.abs(coefs))

    assert (np.abs(coefs[t]) <= 1.0), "Stage1Filter coefficients must be scaled to the range [-1.0, 1.0]"

    # The decimation factor
    self.dec_factor = decimation_factor

    # The coefficients themselves
    self.coefs = coefs

    # The scale factor for scaling to int16
    self.scale_int16 = Stage1Filter.INT16_MAX_COEFFICIENT / self.coefs[t]

    # The int16 filter coefficients
    # self.scale_int16 = (Stage1Filter.INT16_MAX_COEFFICIENT * (1 if coefs[t] >= 0 else -1)) / coefs[t]
    self.coefs_int16 = np.round(self.scale_int16 * self.coefs).astype(np.int16)

    # The bipolar {-1,1} filter coefficient representation
    self.coefs_bipolar = util.int16_vect_to_bipolar_matrix(self.coefs_int16, util.int16_dual)

    # The binary {1,0} filter coefficient representation
    #  (note that the binary matrix is from the bipolar transposed)
    self.coefs_binary = (1-self.coefs_bipolar.T)//2

  @property
  def DecimationFactor(self):
    return self.dec_factor

  @property
  def TapCount(self):
    return len(self.coefs)
  
  @property
  def BlockCount(self):
    return len(self.coefs) // Stage1Filter.BLOCK_SIZE

  @property
  def Coef(self):
    return self.coefs

  @property
  def CoefInt16(self):
    return self.coefs_int16

  @property
  def ScaleInt16(self):
    return self.scale_int16
  
  @property
  def CoefBipolar(self):
    return self.coefs_bipolar
  
  @property
  def CoefBinary(self):
    return self.coefs_binary

  def _pad_input(self, sig_in):
    if sig_in.ndim == 1:
      sig_in = sig_in[np.newaxis,:]
    CHANS,SAMP_IN = sig_in.shape
    Q = self.DecimationFactor
    P = self.TapCount - Q
    S = np.zeros((CHANS, P+SAMP_IN), dtype=sig_in.dtype)
    S[:,:P] = (2*(np.arange(P) % 2) - 1).astype(sig_in.dtype)
    S[:,P:] = sig_in
    return S

  def FilterFloat(self, pdm_signal: np.ndarray) -> np.ndarray:
    if pdm_signal.ndim == 1:
      pdm_signal = pdm_signal[np.newaxis,:]
    CHANS, SAMPS_IN = pdm_signal.shape
    Q = self.DecimationFactor
    N_pcm = SAMPS_IN // self.DecimationFactor
    
    S = self._pad_input(pdm_signal)
    coefs = self.Coef.astype(np.float)[:,np.newaxis]
    res = np.zeros((CHANS,N_pcm), dtype=np.float)
    for k in range(N_pcm):
      x = S[:,Q*k:Q*k+self.TapCount]
      res[:,k] = np.matmul(x, coefs).squeeze()
    return res
  
  def FilterInt16(self, pdm_signal: np.ndarray) -> np.ndarray:
    if pdm_signal.ndim == 1:
      pdm_signal = pdm_signal[np.newaxis,:]
    CHANS, SAMPS_IN = pdm_signal.shape
    Q = self.DecimationFactor
    N_pcm = SAMPS_IN // self.DecimationFactor
    
    S = self._pad_input(pdm_signal)
    coefs = self.CoefInt16.astype(np.int32)[:,np.newaxis]
    res = np.zeros((CHANS, N_pcm), dtype=np.int32)
    for k in range(N_pcm):
      x = S[:,Q*k:Q*k+self.TapCount]
      res[:,k] = np.matmul(x, coefs).squeeze()
    return (res << 8) # stage1 does this on device
  
  def ToXCoreCoefArray(self):

    B = self.CoefBinary
    B = B.reshape((B.size // 256, 8, 32)).astype(np.uint32)
    B = np.flip(B, axis=1)
    B = np.flip(B, axis=2)
    B = B.reshape((B.size // 32, 32))
    N = B.shape[0]
    y = np.zeros(N, dtype=np.uint32)
    
    F = (2**np.arange(32)).astype(np.uint32)[::-1]
    
    for k in range(N):
      y[k] = np.dot(F, B[k,:])
    
    return y





class Stage2Filter(object):

  INT32_MAX_COEFFICIENT = (2**31)-1

  def __init__(self, coefs: np.ndarray, decimation_factor: int):

    assert (coefs.ndim == 1), "Stage2Filter coefs must be a single dimensional ndarray"

    t = np.argmax(np.abs(coefs))

    assert (np.abs(coefs[t]) <= 1.0), "Stage2Filter coefficients must be scaled to the range [-1.0, 1.0]"

    # The decimation factor
    self.dec_factor = decimation_factor

    # The coefficients themselves
    self.coefs = coefs

    # The scale factor for scaling to int32
    self.scale_int32 = Stage2Filter.INT32_MAX_COEFFICIENT / self.coefs[t]

    # The int32 filter coefficients
    self.coefs_int32 = np.round(self.scale_int32 * self.coefs).astype(np.int32)

    # The right-shift required for fixed-point output scaling
    #  (the 0x7FFFFF00 is the largest value a 1-block Stage1 filter can output)
    max_samp_out = np.dot( np.int64( 0x7FFFFF00) * np.sign(self.coefs_int32).astype(np.int64), 
                           np.round(self.coefs_int32 / 2**30).astype(np.int64) )
    frac_shr = np.log2(max_samp_out) - 30
    self.shr_int32 = np.int(np.floor(frac_shr))


  @property
  def DecimationFactor(self):
    return self.dec_factor

  @property
  def TapCount(self):
    return len(self.coefs)

  @property
  def Coef(self):
    return self.coefs

  @property
  def CoefInt32(self):
    return self.coefs_int32

  @property
  def ScaleInt32(self):
    return self.scale_int32
  
  @property
  def ShrInt32(self):
    return self.shr_int32
  
  def _pad_input(self, sig_in):
    if sig_in.ndim == 1:
      sig_in = sig_in[np.newaxis,:]
    CHANS,SAMP_IN = sig_in.shape
    Q = self.DecimationFactor
    P = self.TapCount - Q
    S = np.zeros((CHANS, P+SAMP_IN), dtype=sig_in.dtype)
    S[:,P:] = sig_in
    return S

  def FilterFloat(self, signal_in: np.ndarray) -> np.ndarray:
    if signal_in.ndim == 1:
      signal_in = signal_in[np.newaxis,:]
    CHANS, SAMPS_IN = signal_in.shape
    Q = self.DecimationFactor
    SAMPS_OUT = SAMPS_IN // self.DecimationFactor

    S = self._pad_input(signal_in)
    res = np.zeros((CHANS,SAMPS_OUT), dtype=np.float)
    coefs = np.flip(self.Coef.astype(np.float))[:,np.newaxis]
    for k in range(SAMPS_OUT):
      x = S[:,Q*k:Q*k+self.TapCount]
      res[:,k] = np.matmul( x, coefs ).squeeze()
    return res

  def FilterInt32(self, signal_in: np.ndarray) -> np.ndarray:
    if signal_in.ndim == 1:
      signal_in = signal_in[np.newaxis,:]
    CHANS, SAMPS_IN = signal_in.shape
    Q = self.DecimationFactor
    SAMPS_OUT = SAMPS_IN // self.DecimationFactor

    S = self._pad_input(signal_in)
    res = np.zeros((CHANS,SAMPS_OUT), dtype=np.int32)
    coefs = np.flip(self.CoefInt32.astype(np.int64))[:,np.newaxis]
    for k in range(SAMPS_OUT):
      x = S[:,Q*k:Q*k+self.TapCount]
      p = np.matmul(x, coefs)
      # astew: Note that this is not precisely the logic used in lib_xcore_math to
      #        apply the second stage filter (via xs3_filter_s32()). But I'd 
      #        rather compare device behavior to the correct result (which this
      #        gives) than get bit-identical results in a way that masks the fact
      #        that the filter isn't perfect.
      res[:,k] = np.round( p * 2**(-30-self.shr_int32) ).astype(np.int32).squeeze()
    return res
  

class TwoStageFilter(object):

  def __init__(self, s1_filter: Stage1Filter, s2_filter: Stage2Filter):
    self.s1 = s1_filter
    self.s2 = s2_filter

  @property
  def DecimationFactor(self):
    return self.s1.DecimationFactor * self.s2.DecimationFactor

  def Filter(self, pdm_signal: np.ndarray) -> np.ndarray:
    s1_output = self.s1.FilterInt16(pdm_signal)
    return self.s2.FilterInt32(s1_output)

    


def load(coef_file: str, /, truncate_s1=False):

  with open(coef_file, "rb") as pkl_file:
    # Split the pickled data into the stage1 and stage2 values
    stage1, stage2 = pkl.load(pkl_file)

  if truncate_s1:
    p = len(stage1[0]) % Stage1Filter.BLOCK_SIZE
    stage1[0] = stage1[0][:-p]

  s1_filter = Stage1Filter(stage1[0], stage1[1])
  s2_filter = Stage2Filter(stage2[0], stage2[1])
  return s1_filter, s2_filter



