// Copyright Vessel contributors. Licensed under Apache-2.0. See LICENSE.

#include "Registry/VesselResult.h"

const TCHAR* VesselResultCodeToString(EVesselResultCode Code)
{
	switch (Code)
	{
		case EVesselResultCode::Ok:                return TEXT("Ok");
		case EVesselResultCode::ValidationError:   return TEXT("ValidationError");
		case EVesselResultCode::NotFound:          return TEXT("NotFound");
		case EVesselResultCode::IoError:           return TEXT("IoError");
		case EVesselResultCode::Timeout:           return TEXT("Timeout");
		case EVesselResultCode::PermissionDenied:  return TEXT("PermissionDenied");
		case EVesselResultCode::Internal:          return TEXT("Internal");
		case EVesselResultCode::UserRejected:      return TEXT("UserRejected");
	}
	return TEXT("Unknown");
}
