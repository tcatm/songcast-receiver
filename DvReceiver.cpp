#include "DvReceiver.h"
#include <OpenHome/Types.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Net/C/DviDeviceC.h>
#include <OpenHome/Net/Core/DvProvider.h>
#include <OpenHome/Net/C/OhNet.h>
#include <OpenHome/Net/Core/DvInvocationResponse.h>
#include <OpenHome/Net/Private/Service.h>
#include <OpenHome/Net/Private/FunctorDviInvocation.h>
#include <OpenHome/Net/C/DvInvocation.h>
#include <OpenHome/Net/C/DvInvocationPrivate.h>
#include <OpenHome/Net/Private/DviStack.h>

using namespace OpenHome;
using namespace OpenHome::Net;

class DvProviderOpenhomeOrgReceiver1C : public DvProvider
{
public:
    DvProviderOpenhomeOrgReceiver1C(DvDeviceC aDevice);
    TBool SetPropertyUri(const Brx& aValue);
    TBool SetPropertyMetadata(const Brx& aValue);
    TBool SetPropertyTransportState(const Brx& aValue);
    TBool SetPropertyProtocolInfo(const Brx& aValue);
    void SetSetSenderCallback(CallbackReceiver1SetSender aCallback, void* aPtr);
    void SetStopCallback(CallbackReceiver1Stop aCallback, void* aPtr);
private:
    void EnableActionPlay();
    void EnableActionStop();
    void EnableActionSetSender();
    void EnableActionSender();
    void EnableActionTransportState();
    void EnableActionProtocolInfo();
    void DoPlay(IDviInvocation& aInvocation);
    void DoStop(IDviInvocation& aInvocation);
    void DoSetSender(IDviInvocation& aInvocation);
    void DoSender(IDviInvocation& aInvocation);
    void DoTransportState(IDviInvocation& aInvocation);
    void DoProtocolInfo(IDviInvocation& aInvocation);
private:
    CallbackReceiver1SetSender iCallbackSetSender;
    void *iPtrSetSender;

    CallbackReceiver1Stop iCallbackStop;
    void *iPtrStop;

    PropertyString* iPropertyUri;
    PropertyString* iPropertyMetadata;
    PropertyString* iPropertyTransportState;
    PropertyString* iPropertyProtocolInfo;
};

DvProviderOpenhomeOrgReceiver1C::DvProviderOpenhomeOrgReceiver1C(DvDeviceC aDevice)
    : DvProvider(DviDeviceC::DeviceFromHandle(aDevice)->Device(), "av.openhome.org", "Receiver", 1)
{
    iPropertyUri = new PropertyString(new ParameterString("Uri"));
    iService->AddProperty(iPropertyUri); // passes ownership

    iPropertyMetadata = new PropertyString(new ParameterString("Metadata"));
    iService->AddProperty(iPropertyMetadata); // passes ownership

    iPropertyTransportState = new PropertyString(new ParameterString("TransportState"));
    iService->AddProperty(iPropertyTransportState); // passes ownership

    iPropertyProtocolInfo = new PropertyString(new ParameterString("ProtocolInfo"));
    iService->AddProperty(iPropertyProtocolInfo); // passes ownership

    SetPropertyUri(Brhz(""));
    SetPropertyMetadata(Brhz(""));
    SetPropertyTransportState(Brhz("Stopped"));
    SetPropertyProtocolInfo(Brhz("ohz:*:*:*,ohm:*:*:*,ohu:*:*:*"));

    EnableActionPlay();
    EnableActionStop();
    EnableActionSetSender();
    EnableActionSender();
    EnableActionProtocolInfo();
    EnableActionTransportState();
}

TBool DvProviderOpenhomeOrgReceiver1C::SetPropertyUri(const Brx& aValue)
{
    ASSERT(iPropertyUri != NULL);
    return SetPropertyString(*iPropertyUri, aValue);
}

TBool DvProviderOpenhomeOrgReceiver1C::SetPropertyMetadata(const Brx& aValue)
{
    ASSERT(iPropertyMetadata != NULL);
    return SetPropertyString(*iPropertyMetadata, aValue);
}

TBool DvProviderOpenhomeOrgReceiver1C::SetPropertyTransportState(const Brx& aValue)
{
    ASSERT(iPropertyTransportState != NULL);
    return SetPropertyString(*iPropertyTransportState, aValue);
}

TBool DvProviderOpenhomeOrgReceiver1C::SetPropertyProtocolInfo(const Brx& aValue)
{
    ASSERT(iPropertyProtocolInfo != NULL);
    return SetPropertyString(*iPropertyProtocolInfo, aValue);
}

void DvProviderOpenhomeOrgReceiver1C::EnableActionPlay()
{
    OpenHome::Net::Action* action = new OpenHome::Net::Action("Play");
    FunctorDviInvocation functor = MakeFunctorDviInvocation(*this, &DvProviderOpenhomeOrgReceiver1C::DoPlay);
    iService->AddAction(action, functor);
}

void DvProviderOpenhomeOrgReceiver1C::EnableActionStop()
{
    OpenHome::Net::Action* action = new OpenHome::Net::Action("Stop");
    FunctorDviInvocation functor = MakeFunctorDviInvocation(*this, &DvProviderOpenhomeOrgReceiver1C::DoStop);
    iService->AddAction(action, functor);
}

void DvProviderOpenhomeOrgReceiver1C::EnableActionSetSender()
{
    OpenHome::Net::Action* action = new OpenHome::Net::Action("SetSender");
    action->AddInputParameter(new ParameterRelated("Uri", *iPropertyUri));
    action->AddInputParameter(new ParameterRelated("Metadata", *iPropertyMetadata));
    FunctorDviInvocation functor = MakeFunctorDviInvocation(*this, &DvProviderOpenhomeOrgReceiver1C::DoSetSender);
    iService->AddAction(action, functor);
}

void DvProviderOpenhomeOrgReceiver1C::EnableActionSender()
{
    OpenHome::Net::Action* action = new OpenHome::Net::Action("Sender");
    action->AddOutputParameter(new ParameterRelated("Uri", *iPropertyUri));
    action->AddOutputParameter(new ParameterRelated("Metadata", *iPropertyMetadata));
    FunctorDviInvocation functor = MakeFunctorDviInvocation(*this, &DvProviderOpenhomeOrgReceiver1C::DoSender);
    iService->AddAction(action, functor);
}

void DvProviderOpenhomeOrgReceiver1C::EnableActionTransportState()
{
    OpenHome::Net::Action* action = new OpenHome::Net::Action("TransportState");
    action->AddOutputParameter(new ParameterRelated("Value", *iPropertyTransportState));
    FunctorDviInvocation functor = MakeFunctorDviInvocation(*this, &DvProviderOpenhomeOrgReceiver1C::DoTransportState);
    iService->AddAction(action, functor);
}

void DvProviderOpenhomeOrgReceiver1C::EnableActionProtocolInfo()
{
    OpenHome::Net::Action* action = new OpenHome::Net::Action("ProtocolInfo");
    action->AddOutputParameter(new ParameterRelated("Value", *iPropertyProtocolInfo));
    FunctorDviInvocation functor = MakeFunctorDviInvocation(*this, &DvProviderOpenhomeOrgReceiver1C::DoProtocolInfo);
    iService->AddAction(action, functor);
}

void DvProviderOpenhomeOrgReceiver1C::DoPlay(IDviInvocation& aInvocation)
{
    DvInvocationCPrivate invocationWrapper(aInvocation);
    IDvInvocationC* invocationC;
    void* invocationCPtr;
    invocationWrapper.GetInvocationC(&invocationC, &invocationCPtr);
    aInvocation.InvocationReadStart();
    aInvocation.InvocationReadEnd();
    DviInvocation invocation(aInvocation);

    printf("Receiver Play\n");

    invocation.StartResponse();
    invocation.EndResponse();
}

void DvProviderOpenhomeOrgReceiver1C::DoStop(IDviInvocation& aInvocation)
{
    DvInvocationCPrivate invocationWrapper(aInvocation);
    IDvInvocationC* invocationC;
    void* invocationCPtr;
    invocationWrapper.GetInvocationC(&invocationC, &invocationCPtr);
    aInvocation.InvocationReadStart();
    aInvocation.InvocationReadEnd();
    DviInvocation invocation(aInvocation);

    ASSERT(iCallbackStop != NULL);
    if (0 != iCallbackStop(iPtrStop, invocationC, invocationCPtr)) {
        invocation.Error(502, Brn("Action failed"));
        return;
    }

    invocation.StartResponse();
    invocation.EndResponse();
}

void DvProviderOpenhomeOrgReceiver1C::DoSetSender(IDviInvocation& aInvocation)
{
    DvInvocationCPrivate invocationWrapper(aInvocation);
    IDvInvocationC* invocationC;
    void* invocationCPtr;
    invocationWrapper.GetInvocationC(&invocationC, &invocationCPtr);
    aInvocation.InvocationReadStart();
    Brhz Uri, Metadata;
    aInvocation.InvocationReadString("Uri", Uri);
    aInvocation.InvocationReadString("Metadata", Metadata);
    aInvocation.InvocationReadEnd();
    DviInvocation invocation(aInvocation);

    ASSERT(iCallbackSetSender != NULL);
    if (0 != iCallbackSetSender(iPtrSetSender, invocationC, invocationCPtr, (const char*)Uri.Ptr())) {
        invocation.Error(502, Brn("Action failed"));
        return;
    }

    SetPropertyString(*iPropertyUri, Uri);
    SetPropertyString(*iPropertyMetadata, Metadata);

    invocation.StartResponse();
    invocation.EndResponse();
}

void DvProviderOpenhomeOrgReceiver1C::DoSender(IDviInvocation& aInvocation)
{
    DvInvocationCPrivate invocationWrapper(aInvocation);
    IDvInvocationC* invocationC;
    void* invocationCPtr;
    invocationWrapper.GetInvocationC(&invocationC, &invocationCPtr);
    aInvocation.InvocationReadStart();
    aInvocation.InvocationReadEnd();
    DviInvocation invocation(aInvocation);

    DviInvocationResponseString respUri(aInvocation, "Uri");
    DviInvocationResponseString respMetadata(aInvocation, "Metadata");

    invocation.StartResponse();
    respUri.Write(iPropertyUri->Value());
    respUri.WriteFlush();
    respMetadata.Write(iPropertyMetadata->Value());
    respMetadata.WriteFlush();
    invocation.EndResponse();
}

void DvProviderOpenhomeOrgReceiver1C::DoTransportState(IDviInvocation& aInvocation)
{
    DvInvocationCPrivate invocationWrapper(aInvocation);
    IDvInvocationC* invocationC;
    void* invocationCPtr;
    invocationWrapper.GetInvocationC(&invocationC, &invocationCPtr);
    aInvocation.InvocationReadStart();
    aInvocation.InvocationReadEnd();
    DviInvocation invocation(aInvocation);

    DviInvocationResponseString respValue(aInvocation, "TransportState");

    invocation.StartResponse();
    respValue.Write(iPropertyTransportState->Value());
    respValue.WriteFlush();
    invocation.EndResponse();
}

void DvProviderOpenhomeOrgReceiver1C::DoProtocolInfo(IDviInvocation& aInvocation)
{
    DvInvocationCPrivate invocationWrapper(aInvocation);
    IDvInvocationC* invocationC;
    void* invocationCPtr;
    invocationWrapper.GetInvocationC(&invocationC, &invocationCPtr);
    aInvocation.InvocationReadStart();
    aInvocation.InvocationReadEnd();
    DviInvocation invocation(aInvocation);

    DviInvocationResponseString respValue(aInvocation, "ProtocolInfo");

    invocation.StartResponse();
    respValue.Write(iPropertyProtocolInfo->Value());
    respValue.WriteFlush();
    invocation.EndResponse();
}

void DvProviderOpenhomeOrgReceiver1C::SetSetSenderCallback(CallbackReceiver1SetSender aCallback, void* aPtr)
{
    iCallbackSetSender = aCallback;
    iPtrSetSender = aPtr;
}

void DvProviderOpenhomeOrgReceiver1C::SetStopCallback(CallbackReceiver1Stop aCallback, void* aPtr)
{
    iCallbackStop = aCallback;
    iPtrStop = aPtr;
}

THandle STDCALL DvProviderOpenhomeOrgReceiver1Create(DvDeviceC aDevice)
{
    return new DvProviderOpenhomeOrgReceiver1C(aDevice);
}

void STDCALL DvProviderAvOpenhomeOrgReceiver1Destroy(THandle aProvider)
{
    delete reinterpret_cast<DvProviderOpenhomeOrgReceiver1C*>(aProvider);
}

void STDCALL DvProviderAvOpenhomeOrgReceiver1SetSetSenderCallback(THandle aProvider, CallbackReceiver1SetSender aCallback, void* aPtr)
{
    reinterpret_cast<DvProviderOpenhomeOrgReceiver1C*>(aProvider)->SetSetSenderCallback(aCallback, aPtr);
}

void STDCALL DvProviderAvOpenhomeOrgReceiver1SetStopCallback(THandle aProvider, CallbackReceiver1Stop aCallback, void* aPtr)
{
    reinterpret_cast<DvProviderOpenhomeOrgReceiver1C*>(aProvider)->SetStopCallback(aCallback, aPtr);
}

void STDCALL DvProviderAvOpenhomeOrgReceiver1SetPropertyTransportState(THandle aProvider, const char *aValue)
{
    Brhz buf(aValue);
    reinterpret_cast<DvProviderOpenhomeOrgReceiver1C*>(aProvider)->SetPropertyTransportState(buf);
}