#include "DvInfo.h"
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

class DvProviderOpenhomeOrgInfo1C : public DvProvider
{
public:
    DvProviderOpenhomeOrgInfo1C(DvDeviceC aDevice);
    TBool SetPropertyTrackCount(TUint aValue);
    void GetPropertyTrackCount(TUint& aValue);
    TBool SetPropertyDetailsCount(TUint aValue);
    void GetPropertyDetailsCount(TUint& aValue);
    TBool SetPropertyMetatextCount(TUint aValue);
    void GetPropertyMetatextCount(TUint& aValue);
    TBool SetPropertyDuration(TUint aValue);
    void GetPropertyDuration(TUint& aValue);
    TBool SetPropertyBitRate(TUint aValue);
    void GetPropertyBitRate(TUint& aValue);
    TBool SetPropertyBitDepth(TUint aValue);
    void GetPropertyBitDepth(TUint& aValue);
    TBool SetPropertySampleRate(TUint aValue);
    void GetPropertySampleRate(TUint& aValue);
    TBool SetPropertyLossless(TBool aValue);
    void GetPropertyLossless(TBool& aValue);
    TBool SetPropertyUri(const Brx& aValue);
    void GetPropertyUri(Brhz& aValue);
    TBool SetPropertyMetadata(const Brx& aValue);
    void GetPropertyMetadata(Brhz& aValue);
    TBool SetPropertyMetatext(const Brx& aValue);
    void GetPropertyMetatext(Brhz& aValue);
    TBool SetPropertyCodecName(const Brx& aValue);
    void GetPropertyCodecName(Brhz& aValue);
    void EnablePropertyTrackCount();
    void EnablePropertyDetailsCount();
    void EnablePropertyMetatextCount();
    void EnablePropertyDuration();
    void EnablePropertyBitRate();
    void EnablePropertyBitDepth();
    void EnablePropertySampleRate();
    void EnablePropertyLossless();
    void EnablePropertyUri();
    void EnablePropertyMetadata();
    void EnablePropertyMetatext();
    void EnablePropertyCodecName();
    void EnableActionCounters();
    void EnableActionTrack();
    void EnableActionDetails();
    void EnableActionMetatext();
private:
    void DoCounters(IDviInvocation& aInvocation);
    void DoTrack(IDviInvocation& aInvocation);
    void DoDetails(IDviInvocation& aInvocation);
    void DoMetatext(IDviInvocation& aInvocation);
private:
    PropertyUint* iPropertyTrackCount;
    PropertyUint* iPropertyDetailsCount;
    PropertyUint* iPropertyMetatextCount;
    PropertyUint* iPropertyDuration;
    PropertyUint* iPropertyBitRate;
    PropertyUint* iPropertyBitDepth;
    PropertyUint* iPropertySampleRate;
    PropertyBool* iPropertyLossless;
    PropertyString* iPropertyUri;
    PropertyString* iPropertyMetadata;
    PropertyString* iPropertyMetatext;
    PropertyString* iPropertyCodecName;
};

DvProviderOpenhomeOrgInfo1C::DvProviderOpenhomeOrgInfo1C(DvDeviceC aDevice)
    : DvProvider(DviDeviceC::DeviceFromHandle(aDevice)->Device(), "av.openhome.org", "Info", 1)
{
    EnablePropertyTrackCount();
    EnablePropertyDetailsCount();
    EnablePropertyMetatextCount();
    EnablePropertyDuration();
    EnablePropertyBitRate();
    EnablePropertyBitDepth();
    EnablePropertySampleRate();
    EnablePropertyLossless();
    EnablePropertyUri();
    EnablePropertyMetadata();
    EnablePropertyMetatext();
    EnablePropertyCodecName();

    SetPropertyTrackCount(0);
    SetPropertyDetailsCount(0);
    SetPropertyMetatextCount(0);
    SetPropertyDuration(0);
    SetPropertyBitRate(0);
    SetPropertyBitDepth(0);
    SetPropertySampleRate(0);
    SetPropertyLossless(true);
    SetPropertyUri(Brhz("Blubb"));
    SetPropertyMetadata(Brhz("Qux"));
    SetPropertyMetatext(Brhz("Bar"));
    SetPropertyCodecName(Brhz("Foo"));

    EnableActionCounters();
    EnableActionTrack();
    EnableActionDetails();
    EnableActionMetatext();
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertyTrackCount(TUint aValue)
{
    ASSERT(iPropertyTrackCount != NULL);
    return SetPropertyUint(*iPropertyTrackCount, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertyTrackCount(TUint& aValue)
{
    ASSERT(iPropertyTrackCount != NULL);
    aValue = iPropertyTrackCount->Value();
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertyDetailsCount(TUint aValue)
{
    ASSERT(iPropertyDetailsCount != NULL);
    return SetPropertyUint(*iPropertyDetailsCount, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertyDetailsCount(TUint& aValue)
{
    ASSERT(iPropertyDetailsCount != NULL);
    aValue = iPropertyDetailsCount->Value();
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertyMetatextCount(TUint aValue)
{
    ASSERT(iPropertyMetatextCount != NULL);
    return SetPropertyUint(*iPropertyMetatextCount, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertyMetatextCount(TUint& aValue)
{
    ASSERT(iPropertyMetatextCount != NULL);
    aValue = iPropertyMetatextCount->Value();
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertyDuration(TUint aValue)
{
    ASSERT(iPropertyDuration != NULL);
    return SetPropertyUint(*iPropertyDuration, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertyDuration(TUint& aValue)
{
    ASSERT(iPropertyDuration != NULL);
    aValue = iPropertyDuration->Value();
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertyBitRate(TUint aValue)
{
    ASSERT(iPropertyBitRate != NULL);
    return SetPropertyUint(*iPropertyBitRate, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertyBitRate(TUint& aValue)
{
    ASSERT(iPropertyBitRate != NULL);
    aValue = iPropertyBitRate->Value();
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertyBitDepth(TUint aValue)
{
    ASSERT(iPropertyBitDepth != NULL);
    return SetPropertyUint(*iPropertyBitDepth, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertyBitDepth(TUint& aValue)
{
    ASSERT(iPropertyBitDepth != NULL);
    aValue = iPropertyBitDepth->Value();
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertySampleRate(TUint aValue)
{
    ASSERT(iPropertySampleRate != NULL);
    return SetPropertyUint(*iPropertySampleRate, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertySampleRate(TUint& aValue)
{
    ASSERT(iPropertySampleRate != NULL);
    aValue = iPropertySampleRate->Value();
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertyLossless(TBool aValue)
{
    ASSERT(iPropertyLossless != NULL);
    return SetPropertyBool(*iPropertyLossless, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertyLossless(TBool& aValue)
{
    ASSERT(iPropertyLossless != NULL);
    aValue = iPropertyLossless->Value();
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertyUri(const Brx& aValue)
{
    ASSERT(iPropertyUri != NULL);
    return SetPropertyString(*iPropertyUri, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertyUri(Brhz& aValue)
{
    ASSERT(iPropertyUri != NULL);
    aValue.Set(iPropertyUri->Value());
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertyMetadata(const Brx& aValue)
{
    ASSERT(iPropertyMetadata != NULL);
    return SetPropertyString(*iPropertyMetadata, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertyMetadata(Brhz& aValue)
{
    ASSERT(iPropertyMetadata != NULL);
    aValue.Set(iPropertyMetadata->Value());
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertyMetatext(const Brx& aValue)
{
    ASSERT(iPropertyMetatext != NULL);
    return SetPropertyString(*iPropertyMetatext, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertyMetatext(Brhz& aValue)
{
    ASSERT(iPropertyMetatext != NULL);
    aValue.Set(iPropertyMetatext->Value());
}

TBool DvProviderOpenhomeOrgInfo1C::SetPropertyCodecName(const Brx& aValue)
{
    ASSERT(iPropertyCodecName != NULL);
    return SetPropertyString(*iPropertyCodecName, aValue);
}

void DvProviderOpenhomeOrgInfo1C::GetPropertyCodecName(Brhz& aValue)
{
    ASSERT(iPropertyCodecName != NULL);
    aValue.Set(iPropertyCodecName->Value());
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertyTrackCount()
{
    iPropertyTrackCount = new PropertyUint(new ParameterUint("TrackCount"));
    iService->AddProperty(iPropertyTrackCount); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertyDetailsCount()
{
    iPropertyDetailsCount = new PropertyUint(new ParameterUint("DetailsCount"));
    iService->AddProperty(iPropertyDetailsCount); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertyMetatextCount()
{
    iPropertyMetatextCount = new PropertyUint(new ParameterUint("MetatextCount"));
    iService->AddProperty(iPropertyMetatextCount); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertyDuration()
{
    iPropertyDuration = new PropertyUint(new ParameterUint("Duration"));
    iService->AddProperty(iPropertyDuration); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertyBitRate()
{
    iPropertyBitRate = new PropertyUint(new ParameterUint("BitRate"));
    iService->AddProperty(iPropertyBitRate); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertyBitDepth()
{
    iPropertyBitDepth = new PropertyUint(new ParameterUint("BitDepth"));
    iService->AddProperty(iPropertyBitDepth); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertySampleRate()
{
    iPropertySampleRate = new PropertyUint(new ParameterUint("SampleRate"));
    iService->AddProperty(iPropertySampleRate); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertyLossless()
{
    iPropertyLossless = new PropertyBool(new ParameterBool("Lossless"));
    iService->AddProperty(iPropertyLossless); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertyUri()
{
    iPropertyUri = new PropertyString(new ParameterString("Uri"));
    iService->AddProperty(iPropertyUri); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertyMetadata()
{
    iPropertyMetadata = new PropertyString(new ParameterString("Metadata"));
    iService->AddProperty(iPropertyMetadata); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertyMetatext()
{
    iPropertyMetatext = new PropertyString(new ParameterString("Metatext"));
    iService->AddProperty(iPropertyMetatext); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnablePropertyCodecName()
{
    iPropertyCodecName = new PropertyString(new ParameterString("CodecName"));
    iService->AddProperty(iPropertyCodecName); // passes ownership
}

void DvProviderOpenhomeOrgInfo1C::EnableActionCounters()
{
    OpenHome::Net::Action* action = new OpenHome::Net::Action("Counters");
    action->AddOutputParameter(new ParameterRelated("TrackCount", *iPropertyTrackCount));
    action->AddOutputParameter(new ParameterRelated("DetailsCount", *iPropertyDetailsCount));
    action->AddOutputParameter(new ParameterRelated("MetatextCount", *iPropertyMetatextCount));
    FunctorDviInvocation functor = MakeFunctorDviInvocation(*this, &DvProviderOpenhomeOrgInfo1C::DoCounters);
    iService->AddAction(action, functor);
}

void DvProviderOpenhomeOrgInfo1C::EnableActionTrack()
{
    OpenHome::Net::Action* action = new OpenHome::Net::Action("Track");
    action->AddOutputParameter(new ParameterRelated("Uri", *iPropertyUri));
    action->AddOutputParameter(new ParameterRelated("Metadata", *iPropertyMetadata));
    FunctorDviInvocation functor = MakeFunctorDviInvocation(*this, &DvProviderOpenhomeOrgInfo1C::DoTrack);
    iService->AddAction(action, functor);
}

void DvProviderOpenhomeOrgInfo1C::EnableActionDetails()
{
    OpenHome::Net::Action* action = new OpenHome::Net::Action("Details");
    action->AddOutputParameter(new ParameterRelated("Duration", *iPropertyDuration));
    action->AddOutputParameter(new ParameterRelated("BitRate", *iPropertyBitRate));
    action->AddOutputParameter(new ParameterRelated("BitDepth", *iPropertyBitDepth));
    action->AddOutputParameter(new ParameterRelated("SampleRate", *iPropertySampleRate));
    action->AddOutputParameter(new ParameterRelated("Lossless", *iPropertyLossless));
    action->AddOutputParameter(new ParameterRelated("CodecName", *iPropertyCodecName));
    FunctorDviInvocation functor = MakeFunctorDviInvocation(*this, &DvProviderOpenhomeOrgInfo1C::DoDetails);
    iService->AddAction(action, functor);
}
void DvProviderOpenhomeOrgInfo1C::EnableActionMetatext()
{
    OpenHome::Net::Action* action = new OpenHome::Net::Action("Metatext");
    action->AddOutputParameter(new ParameterRelated("Value", *iPropertyMetatext));
    FunctorDviInvocation functor = MakeFunctorDviInvocation(*this, &DvProviderOpenhomeOrgInfo1C::DoMetatext);
    iService->AddAction(action, functor);
}

void DvProviderOpenhomeOrgInfo1C::DoCounters(IDviInvocation& aInvocation)
{
    ASSERT(iPropertyTrackCount != NULL);
    ASSERT(iPropertyDetailsCount != NULL);
    ASSERT(iPropertyMetatextCount != NULL);

    DvInvocationCPrivate invocationWrapper(aInvocation);
    IDvInvocationC* invocationC;
    void* invocationCPtr;
    invocationWrapper.GetInvocationC(&invocationC, &invocationCPtr);
    aInvocation.InvocationReadStart();
    aInvocation.InvocationReadEnd();
    DviInvocation invocation(aInvocation);

    DviInvocationResponseUint respTrackCount(aInvocation, "TrackCount");
    DviInvocationResponseUint respDetailsCount(aInvocation, "DetailsCount");
    DviInvocationResponseUint respMetatextCount(aInvocation, "MetatextCount");

    invocation.StartResponse();
    respTrackCount.Write(iPropertyTrackCount->Value());
    respDetailsCount.Write(iPropertyDetailsCount->Value());
    respMetatextCount.Write(iPropertyMetatextCount->Value());
    invocation.EndResponse();
}

void DvProviderOpenhomeOrgInfo1C::DoTrack(IDviInvocation& aInvocation)
{
    ASSERT(iPropertyUri != NULL);
    ASSERT(iPropertyMetadata != NULL);

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

void DvProviderOpenhomeOrgInfo1C::DoDetails(IDviInvocation& aInvocation)
{
    ASSERT(iPropertyDuration != NULL);
    ASSERT(iPropertyBitRate != NULL);
    ASSERT(iPropertyBitDepth != NULL);
    ASSERT(iPropertySampleRate != NULL);
    ASSERT(iPropertyLossless != NULL);
    ASSERT(iPropertyCodecName != NULL);

    DvInvocationCPrivate invocationWrapper(aInvocation);
    IDvInvocationC* invocationC;
    void* invocationCPtr;
    invocationWrapper.GetInvocationC(&invocationC, &invocationCPtr);
    aInvocation.InvocationReadStart();
    aInvocation.InvocationReadEnd();
    DviInvocation invocation(aInvocation);

    DviInvocationResponseUint respDuration(aInvocation, "Duration");
    DviInvocationResponseUint respBitRate(aInvocation, "BitRate");
    DviInvocationResponseUint respBitDepth(aInvocation, "BitDepth");
    DviInvocationResponseUint respSampleRate(aInvocation, "SampleRate");
    DviInvocationResponseBool respLossless(aInvocation, "Lossless");
    DviInvocationResponseString respCodecName(aInvocation, "CodecName");

    invocation.StartResponse();
    respDuration.Write(iPropertyDuration->Value());
    respBitRate.Write(iPropertyBitRate->Value());
    respBitDepth.Write(iPropertyBitDepth->Value());
    respSampleRate.Write(iPropertySampleRate->Value());
    respLossless.Write(iPropertyLossless->Value());
    respCodecName.Write(iPropertyCodecName->Value());
    respCodecName.WriteFlush();
    invocation.EndResponse();
}

void DvProviderOpenhomeOrgInfo1C::DoMetatext(IDviInvocation& aInvocation)
{
    ASSERT(iPropertyMetatext != NULL);

    DvInvocationCPrivate invocationWrapper(aInvocation);
    IDvInvocationC* invocationC;
    void* invocationCPtr;
    invocationWrapper.GetInvocationC(&invocationC, &invocationCPtr);
    aInvocation.InvocationReadStart();
    aInvocation.InvocationReadEnd();
    DviInvocation invocation(aInvocation);

    DviInvocationResponseString respValue(aInvocation, "Value");

    invocation.StartResponse();
    respValue.Write(iPropertyMetatext->Value());
    respValue.WriteFlush();
    invocation.EndResponse();
}

THandle STDCALL DvProviderOpenhomeOrgInfo1Create(DvDeviceC aDevice)
{
    return new DvProviderOpenhomeOrgInfo1C(aDevice);
}

void STDCALL DvProviderAvOpenhomeOrgInfo1Destroy(THandle aProvider)
{
    delete reinterpret_cast<DvProviderOpenhomeOrgInfo1C*>(aProvider);
}