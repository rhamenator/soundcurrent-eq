// SPDX-License-Identifier: MS-PL
// SoundCurrent microphone transport endpoints, using SYSVAD automation tables.
#pragma once
// One kernel stream per direction is required by the SPSC adapter transport.
static PCPIN_DESCRIPTOR ScMicFeedPins[] = {
    {1,1,0,&AutomationSpeakerHostPin,
     {0,NULL,0,NULL,SIZEOF_ARRAY(ScRenderRanges),ScRenderRanges,
      KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_SINK,&KSCATEGORY_AUDIO,NULL,0}},
    {0,0,0,NULL,
     {0,NULL,0,NULL,SIZEOF_ARRAY(SpeakerPinDataRangePointersBridge),SpeakerPinDataRangePointersBridge,
      KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}}
};
static PCFILTER_DESCRIPTOR ScMicFeedDescriptor = {
    0,&AutomationSpeakerWaveFilter,sizeof(PCPIN_DESCRIPTOR),SIZEOF_ARRAY(ScMicFeedPins),ScMicFeedPins,
    sizeof(PCNODE_DESCRIPTOR),0,NULL,
    SIZEOF_ARRAY(ScRenderConnections),ScRenderConnections,0,NULL
};
static PIN_DEVICE_FORMATS_AND_MODES ScMicCaptureFormats[] = {
    {BridgePin,NULL,0,NULL,0},
    {SystemCapturePin,ScRenderFormats,SIZEOF_ARRAY(ScRenderFormats),ScRenderModes,SIZEOF_ARRAY(ScRenderModes)}
};
static PCPIN_DESCRIPTOR ScMicCapturePins[] = {
    {0,0,0,NULL,
     {0,NULL,0,NULL,SIZEOF_ARRAY(MicInPinDataRangePointersBridge),MicInPinDataRangePointersBridge,
      KSPIN_DATAFLOW_IN,KSPIN_COMMUNICATION_NONE,&KSCATEGORY_AUDIO,NULL,0}},
    {1,1,0,NULL,
     {0,NULL,0,NULL,SIZEOF_ARRAY(ScRenderRanges),ScRenderRanges,
      KSPIN_DATAFLOW_OUT,KSPIN_COMMUNICATION_SINK,&KSCATEGORY_AUDIO,&KSAUDFNAME_RECORDING_CONTROL,0}}
};
static PCFILTER_DESCRIPTOR ScMicCaptureDescriptor = {
    0,&AutomationMicInWaveFilter,sizeof(PCPIN_DESCRIPTOR),SIZEOF_ARRAY(ScMicCapturePins),ScMicCapturePins,
    sizeof(PCNODE_DESCRIPTOR),SIZEOF_ARRAY(MicInWaveMiniportNodes),MicInWaveMiniportNodes,
    SIZEOF_ARRAY(MicInWaveMiniportConnections),MicInWaveMiniportConnections,0,NULL
};
