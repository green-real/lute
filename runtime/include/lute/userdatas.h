#pragma once

// all tags count down from 128
const int kDurationTag       = 127;
const int kInstantTag        = 126;
const int kCompilerResultTag = 125;

// FFI tags
const int kFFIStatePointerTag = 120;

const int kFFICArrayTypeTag = 119;
const int kFFICBaseTypeTag = 118;
const int kFFICFuncTypeTag = 117;
const int kFFICPointerTypeTag = 116;
const int kFFICStructTypeTag = 115;

const int kFFICArrayDataTag = 114;
const int kFFICBaseDataTag = 113;
const int kFFICFuncDataTag = 112;
const int kFFICPointerDataTag = 111;
const int kFFICStructDataTag = 110;
