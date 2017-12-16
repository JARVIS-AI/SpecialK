#include <Windows.h>
#include <SpecialK/steam_api.h>
#include <SpecialK/utility.h>
#include <cstdint>

#include <unordered_map>

class ISteamUser;
class IWrapSteamUser;

class ISteamUtils;
class IWrapSteamUtils;

class ISteamController;
class IWrapSteamController;

class ISteamRemoteStorage;
class IWrapSteamRemoteStorage;

class ISteamClient;
class IWrapSteamClient;

#include <concurrent_unordered_map.h>

extern concurrency::concurrent_unordered_map <ISteamUser*,          IWrapSteamUser*>          SK_SteamWrapper_remap_user;
extern concurrency::concurrent_unordered_map <ISteamUtils*,         IWrapSteamUtils*>         SK_SteamWrapper_remap_utils;
extern concurrency::concurrent_unordered_map <ISteamController*,    IWrapSteamController*>    SK_SteamWrapper_remap_controller;
extern concurrency::concurrent_unordered_map <ISteamRemoteStorage*, IWrapSteamRemoteStorage*> SK_SteamWrapper_remap_remotestorage;
       concurrency::concurrent_unordered_map <ISteamClient*,        IWrapSteamClient*>        SK_SteamWrapper_remap_client;


extern
ISteamUser*
SK_SteamWrapper_WrappedClient_GetISteamUser ( ISteamClient *This,
                                              HSteamUser    hSteamUser,
                                              HSteamPipe    hSteamPipe,
                                              const char   *pchVersion );

extern
ISteamController*
SK_SteamWrapper_WrappedClient_GetISteamController ( ISteamClient *This,
                                                    HSteamUser    hSteamUser,
                                                    HSteamPipe    hSteamPipe,
                                                    const char   *pchVersion );

extern
ISteamUtils*
SK_SteamWrapper_WrappedClient_GetISteamUtils ( ISteamClient *This,
                                               HSteamPipe    hSteamPipe,
                                               const char   *pchVersion );

extern
ISteamRemoteStorage*
SK_SteamWrapper_WrappedClient_GetISteamRemoteStorage ( ISteamClient *This,
                                                       HSteamUser    hSteamuser,
                                                       HSteamPipe    hSteamPipe,
                                                       const char   *pchVersion );

class IWrapSteamClient : public ISteamClient
{
public:
  IWrapSteamClient (ISteamClient* pSteamClient) :
                     pRealClient (pSteamClient) {
  };

  virtual HSteamPipe CreateSteamPipe     (void)                                               override { return pRealClient->CreateSteamPipe     (          );                }
  virtual bool       BReleaseSteamPipe   (HSteamPipe   hSteamPipe)                            override { return pRealClient->BReleaseSteamPipe   (hSteamPipe);                }
  virtual HSteamUser ConnectToGlobalUser (HSteamPipe   hSteamPipe)                            override { return pRealClient->ConnectToGlobalUser (hSteamPipe);                }
  virtual HSteamUser CreateLocalUser     (HSteamPipe *phSteamPipe, EAccountType eAccountType) override { return pRealClient->CreateLocalUser     (phSteamPipe, eAccountType); }
  virtual void       ReleaseUser         (HSteamPipe   hSteamPipe, HSteamUser hUser)          override { return pRealClient->ReleaseUser         (hSteamPipe,  hUser);        }



  virtual ISteamUser *GetISteamUser      (       HSteamUser  hSteamUser,
                                                 HSteamPipe  hSteamPipe,
                                           const char       *pchVersion ) override
  {
#if 1
    return SK_SteamWrapper_WrappedClient_GetISteamUser ( pRealClient,
                                                           hSteamUser,
                                                             hSteamPipe,
                                                               pchVersion );
#else
    return pRealClient->GetISteamUser ( hSteamUser,
                                          hSteamPipe,
                                            pchVersion );
#endif
  }



  virtual ISteamGameServer         *GetISteamGameServer          (HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamGameServer         (hSteamUser, hSteamPipe, pchVersion);                                                        }
  virtual void                      SetLocalIPBinding            (uint32 unIP, uint16 usPort)                                           override {
    return pRealClient->SetLocalIPBinding           (unIP, usPort);                                                                              }
  virtual ISteamFriends            *GetISteamFriends             (HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamFriends            (hSteamUser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamUtils              *GetISteamUtils               (HSteamPipe hSteamPipe, const char *pchVersion)                        override
  {
#if 1
    return SK_SteamWrapper_WrappedClient_GetISteamUtils ( pRealClient,
                                                            hSteamPipe,
                                                              pchVersion );                                                                      
#else
    return pRealClient->GetISteamUtils ( hSteamPipe,
                                           pchVersion );
#endif
}
  virtual ISteamMatchmaking        *GetISteamMatchmaking         (HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamMatchmaking        (hSteamUser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamMatchmakingServers *GetISteamMatchmakingServers  (HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamMatchmakingServers (hSteamUser, hSteamPipe, pchVersion);                                                        }
  virtual void                     *GetISteamGenericInterface    (HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamGenericInterface   (hSteamUser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamUserStats          *GetISteamUserStats           (HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamUserStats          (hSteamUser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamGameServerStats    *GetISteamGameServerStats     (HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamGameServerStats    (hSteamuser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamApps               *GetISteamApps                (HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamApps               (hSteamUser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamNetworking         *GetISteamNetworking          (HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamNetworking         (hSteamUser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamRemoteStorage      *GetISteamRemoteStorage       (HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
#if 1
    return SK_SteamWrapper_WrappedClient_GetISteamRemoteStorage ( pRealClient,
                                                                    hSteamuser,
                                                                      hSteamPipe,
                                                                        pchVersion );
#else
    return pRealClient->GetISteamRemoteStorage ( hSteamuser,
                                                   hSteamPipe,
                                                     pchVersion );
#endif
}
  virtual ISteamScreenshots        *GetISteamScreenshots         (HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamScreenshots        (hSteamuser, hSteamPipe, pchVersion);                                                        }
  virtual void                      RunFrame                     (void)                                                                 override {
    return pRealClient->RunFrame                    ();                                                                                          }
  virtual uint32                    GetIPCCallCount              (void)                                                                 override {
    return pRealClient->GetIPCCallCount             ();                                                                                          }
  virtual void                      SetWarningMessageHook        (SteamAPIWarningMessageHook_t pFunction)                               override {
    return pRealClient->SetWarningMessageHook       (pFunction);                                                                                 }
  virtual bool                      BShutdownIfAllPipesClosed    (void)                                                override { return false;  }
  virtual ISteamHTTP               *GetISteamHTTP                (HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamHTTP               (hSteamuser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamUnifiedMessages    *GetISteamUnifiedMessages     (HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamUnifiedMessages    (hSteamuser, hSteamPipe, pchVersion);                                                        }




  virtual ISteamController         *GetISteamController          (HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
#if 0
    return pRealClient->GetISteamController (hSteamUser, hSteamPipe, pchVersion);
#else
    return SK_SteamWrapper_WrappedClient_GetISteamController (pRealClient, hSteamUser, hSteamPipe, pchVersion);
#endif
                                                                                                                                                 }






  virtual ISteamUGC                *GetISteamUGC                 (HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamUGC                (hSteamUser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamAppList            *GetISteamAppList             (HSteamUser hSteamUser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamAppList            (hSteamUser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamMusic              *GetISteamMusic               (HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamMusic              (hSteamuser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamMusicRemote        *GetISteamMusicRemote         (HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamMusicRemote        (hSteamuser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamHTMLSurface        *GetISteamHTMLSurface         (HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamHTMLSurface        (hSteamuser, hSteamPipe, pchVersion);                                                        }

  virtual void                      Set_SteamAPI_CPostAPIResultInProcess           (SteamAPI_PostAPIResultInProcess_t  func) override {
    return pRealClient->Set_SteamAPI_CPostAPIResultInProcess           (func);                                                        }
  virtual void                      Remove_SteamAPI_CPostAPIResultInProcess        (SteamAPI_PostAPIResultInProcess_t  func) override {
    return pRealClient->Remove_SteamAPI_CPostAPIResultInProcess        (func);                                                        }
  virtual void                      Set_SteamAPI_CCheckCallbackRegisteredInProcess (SteamAPI_CheckCallbackRegistered_t func) override {
    return pRealClient->Set_SteamAPI_CCheckCallbackRegisteredInProcess (func); }

  virtual ISteamInventory          *GetISteamInventory           (HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamInventory          (hSteamuser, hSteamPipe, pchVersion);                                                        }
  virtual ISteamVideo              *GetISteamVideo               (HSteamUser hSteamuser, HSteamPipe hSteamPipe, const char *pchVersion) override {
    return pRealClient->GetISteamVideo              (hSteamuser, hSteamPipe, pchVersion);                                                        }

private:
  ISteamClient* pRealClient;
};



using SteamClient_pfn = ISteamClient* (S_CALLTYPE *)(
        void
      );
SteamClient_pfn SteamClient_Original = nullptr;

ISteamClient*
S_CALLTYPE
SteamClient_Detour (void)
{
  SK_RunOnce (
    steam_log.Log ( L"[!] %hs ()",
                      __FUNCTION__ )
  );

  ISteamClient* pClient =
    static_cast <ISteamClient *> ( SteamClient_Original () );

  if (pClient != nullptr)
  {
    if (SK_SteamWrapper_remap_client.count (pClient))
       return SK_SteamWrapper_remap_client [pClient];

    else
    {
      SK_SteamWrapper_remap_client [pClient] =
              new IWrapSteamClient (pClient);

      return SK_SteamWrapper_remap_client [pClient];
    }
  }

  return nullptr;
}


using SteamInternal_CreateInterface_pfn = void*       (S_CALLTYPE *)(
  const char *ver
  );
SteamInternal_CreateInterface_pfn         SteamInternal_CreateInterface_Original = nullptr;


void*
S_CALLTYPE
SteamInternal_CreateInterface_Detour (const char *ver)
{
  dll_log.Log (L"[SteamWrap] SteamInternal_CreateInterface (%hs)", ver);

  if (! lstrcmpA (ver, STEAMCLIENT_INTERFACE_VERSION))
  {
    ISteamClient* pClient =
      static_cast <ISteamClient *> ( SteamInternal_CreateInterface_Original (ver) );

    if (pClient != nullptr)
    {
      if (SK_SteamWrapper_remap_client.count (pClient))
         return SK_SteamWrapper_remap_client [pClient];

      else
      {
        SK_SteamWrapper_remap_client [pClient] =
                new IWrapSteamClient (pClient);

        return SK_SteamWrapper_remap_client [pClient];
      }
    }
  }

  else
  {
    SK_RunOnce (
      steam_log.Log ( L"Game requested unexpected interface version (%s)!",
                        ver )
    );
  }

  return
    SteamInternal_CreateInterface_Original (ver);
}