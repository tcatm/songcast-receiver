#include "DvTime.h"
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

class DvProviderOpenhomeOrgTime1C : public DvProvider
{
public:
    DvProviderOpenhomeOrgTime1C(DvDeviceC aDevice);
    TBool SetPropertyTrackCount(TUint aValue);
    TBool SetPropertyDuration(TUint aValue);
    TBool SetPropertySeconds(TUint aValue);
private:
    void EnableActionTime();
    void DoTime(IDviInvocation& aInvocation);
private:
    PropertyUint* iPropertyTrackCount;
    PropertyUint* iPropertyDuration;
    PropertyUint* iPropertySeconds;
};

DvProviderOpenhomeOrgTime1C::DvProviderOpenhomeOrgTime1C(DvDeviceC aDevice)
    : DvProvider(DviDeviceC::DeviceFromHandle(aDevice)->Device(), "av.openhome.org", "Time", 1)
{
    iPropertyTrackCount = new PropertyUint(new ParameterUint("TrackCount"));
    iService->AddProperty(iPropertyTrackCount); // passes ownership

    iPropertyDuration = new PropertyUint(new ParameterUint("Duration"));
    iService->AddProperty(iPropertyDuration); // passes ownership

    iPropertySeconds = new PropertyUint(new ParameterUint("Seconds"));
    iService->AddProperty(iPropertySeconds); // passes ownership

    SetPropertyTrackCount(0);
    SetPropertyDuration(0);
    SetPropertySeconds(0);

    EnableActionTime();
}

TBool DvProviderOpenhomeOrgTime1C::SetPropertyTrackCount(TUint aValue)
{
    ASSERT(iPropertyTrackCount != NULL);
    return SetPropertyUint(*iPropertyTrackCount, aValue);
}

TBool DvProviderOpenhomeOrgTime1C::SetPropertyDuration(TUint aValue)
{
    ASSERT(iPropertyDuration != NULL);
    return SetPropertyUint(*iPropertyDuration, aValue);
}

TBool DvProviderOpenhomeOrgTime1C::SetPropertySeconds(TUint aValue)
{
    ASSERT(iPropertySeconds != NULL);
    return SetPropertyUint(*iPropertySeconds, aValue);
}

void DvProviderOpenhomeOrgTime1C::EnableActionTime()
{
    OpenHome::Net::Action* action = new OpenHome::Net::Action("Time");
    action->AddOutputParameter(new ParameterRelated("TrackCount", *iPropertyTrackCount));
    action->AddOutputParameter(new ParameterRelated("Duration", *iPropertyDuration));
    action->AddOutputParameter(new ParameterRelated("Seconds", *iPropertySeconds));
    FunctorDviInvocation functor = MakeFunctorDviInvocation(*this, &DvProviderOpenhomeOrgTime1C::DoTime);
    iService->AddAction(action, functor);
}

void DvProviderOpenhomeOrgTime1C::DoTime(IDviInvocation& aInvocation)
{
    ASSERT(iPropertyTrackCount != NULL);
    ASSERT(iPropertyDuration != NULL);
    ASSERT(iPropertySeconds != NULL);

    DvInvocationCPrivate invocationWrapper(aInvocation);
    IDvInvocationC* invocationC;
    void* invocationCPtr;
    invocationWrapper.GetInvocationC(&invocationC, &invocationCPtr);
    aInvocation.InvocationReadStart();
    aInvocation.InvocationReadEnd();
    DviInvocation invocation(aInvocation);

    DviInvocationResponseUint respTrackCount(aInvocation, "TrackCount");
    DviInvocationResponseUint respDuration(aInvocation, "Duration");
    DviInvocationResponseUint respSeconds(aInvocation, "Seconds");

    invocation.StartResponse();
    respTrackCount.Write(iPropertyTrackCount->Value());
    respDuration.Write(iPropertyDuration->Value());
    respSeconds.Write(iPropertySeconds->Value());
    invocation.EndResponse();
}

THandle STDCALL DvProviderOpenhomeOrgTime1Create(DvDeviceC aDevice)
{
    return new DvProviderOpenhomeOrgTime1C(aDevice);
}

void STDCALL DvProviderAvOpenhomeOrgTime1Destroy(THandle aProvider)
{
    delete reinterpret_cast<DvProviderOpenhomeOrgTime1C*>(aProvider);
}