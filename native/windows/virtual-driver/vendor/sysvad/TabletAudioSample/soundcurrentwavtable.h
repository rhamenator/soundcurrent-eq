// SPDX-License-Identifier: MS-PL
// Derived from Microsoft SYSVAD speaker table; copyright Microsoft Corporation.
// SoundCurrent render-only topology: Windows supplies software loopback.
#pragma once
static KSDATAFORMAT_WAVEFORMATEXTENSIBLE ScRenderFormats[] = {
    {{sizeof(KSDATAFORMAT_WAVEFORMATEXTENSIBLE),0,0,0,
      STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),
      STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)},
     {{WAVE_FORMAT_EXTENSIBLE,2,48000,192000,4,16,
       sizeof(WAVEFORMATEXTENSIBLE)-sizeof(WAVEFORMATEX)},16,
       KSAUDIO_SPEAKER_STEREO,STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM)}}
};
static MODE_AND_DEFAULT_FORMAT ScRenderModes[] = {
    {STATIC_AUDIO_SIGNALPROCESSINGMODE_RAW,&ScRenderFormats[0].DataFormat},
    {STATIC_AUDIO_SIGNALPROCESSINGMODE_DEFAULT,&ScRenderFormats[0].DataFormat}
};
static PIN_DEVICE_FORMATS_AND_MODES ScPinFormats[] = {
    {SystemRenderPin,ScRenderFormats,SIZEOF_ARRAY(ScRenderFormats),ScRenderModes,SIZEOF_ARRAY(ScRenderModes)},
    {BridgePin,NULL,0,NULL,0}
};
static KSDATARANGE_AUDIO ScRenderRange = {
    {sizeof(KSDATARANGE_AUDIO),0,0,0,STATICGUIDOF(KSDATAFORMAT_TYPE_AUDIO),
     STATICGUIDOF(KSDATAFORMAT_SUBTYPE_PCM),STATICGUIDOF(KSDATAFORMAT_SPECIFIER_WAVEFORMATEX)},
    2,16,16,48000,48000
};
static PKSDATARANGE ScRenderRanges[] = {reinterpret_cast<PKSDATARANGE>(&ScRenderRange)};
static PCPIN_DESCRIPTOR ScRenderPins[] = {
    {6,6,0,&AutomationSpeakerHostPin,
     {0,NULL,0,NULL,SIZEOF_ARRAY(ScRenderRanges),ScRenderRanges,
      KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_SINK,&KSCATEGORY_AUDIO,NULL,0}},
    {0,0,0,NULL,
     {0,NULL,0,NULL,SIZEOF_ARRAY(SpeakerPinDataRangePointersBridge),SpeakerPinDataRangePointersBridge,
      KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}}
};
static PCCONNECTION_DESCRIPTOR ScRenderConnections[] = {
    {PCFILTER_NODE,0,PCFILTER_NODE,1}
};
static PCFILTER_DESCRIPTOR ScRenderDescriptor = {
    0,&AutomationSpeakerWaveFilter,sizeof(PCPIN_DESCRIPTOR),SIZEOF_ARRAY(ScRenderPins),ScRenderPins,
    sizeof(PCNODE_DESCRIPTOR),0,NULL,
    SIZEOF_ARRAY(ScRenderConnections),ScRenderConnections,0,NULL
};
