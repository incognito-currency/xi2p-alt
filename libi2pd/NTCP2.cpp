#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <stdlib.h>
#include <vector>
#include "Log.h"
#include "I2PEndian.h"
#include "Crypto.h"
#include "Ed25519.h"
#include "Siphash.h"
#include "RouterContext.h"
#include "Transports.h"
#include "NetDb.hpp"
#include "NTCP2.h"

namespace i2p
{
namespace transport
{
	NTCP2Establisher::NTCP2Establisher () 
	{ 
		m_Ctx = BN_CTX_new (); 
		CreateEphemeralKey ();
	}

	NTCP2Establisher::~NTCP2Establisher () 
	{ 
		BN_CTX_free (m_Ctx); 
	}

	void NTCP2Establisher::MixKey (const uint8_t * inputKeyMaterial, uint8_t * derived)
	{
		// temp_key = HMAC-SHA256(ck, input_key_material)
		uint8_t tempKey[32]; unsigned int len;
		HMAC(EVP_sha256(), m_CK, 32, inputKeyMaterial, 32, tempKey, &len); 	
		// ck = HMAC-SHA256(temp_key, byte(0x01)) 
		static uint8_t one[1] =  { 1 };
		HMAC(EVP_sha256(), tempKey, 32, one, 1, m_CK, &len); 	
		// derived = HMAC-SHA256(temp_key, ck || byte(0x02))
		m_CK[32] = 2;
		HMAC(EVP_sha256(), tempKey, 32, m_CK, 33, derived, &len); 	
	}

	void NTCP2Establisher::KeyDerivationFunction1 (const uint8_t * rs, const uint8_t * priv, const uint8_t * pub)
	{
		static const uint8_t protocolNameHash[] = 
		{ 
			0x72, 0xe8, 0x42, 0xc5, 0x45, 0xe1, 0x80, 0x80, 0xd3, 0x9c, 0x44, 0x93, 0xbb, 0x91, 0xd7, 0xed, 
			0xf2, 0x28, 0x98, 0x17, 0x71, 0x21, 0x8c, 0x1f, 0x62, 0x4e, 0x20, 0x6f, 0x28, 0xd3, 0x2f, 0x71 
		}; // SHA256 ("Noise_XKaesobfse+hs2+hs3_25519_ChaChaPoly_SHA256")
		static const uint8_t hh[32] = 
		{ 
			0x49, 0xff, 0x48, 0x3f, 0xc4, 0x04, 0xb9, 0xb2, 0x6b, 0x11, 0x94, 0x36, 0x72, 0xff, 0x05, 0xb5, 
			0x61, 0x27, 0x03, 0x31, 0xba, 0x89, 0xb8, 0xfc, 0x33, 0x15, 0x93, 0x87, 0x57, 0xdd, 0x3d, 0x1e 
		}; // SHA256 (protocolNameHash)
		memcpy (m_CK, protocolNameHash, 32); 
		// h = SHA256(hh || rs)
		SHA256_CTX ctx;
		SHA256_Init (&ctx);
		SHA256_Update (&ctx, hh, 32);			
		SHA256_Update (&ctx, rs, 32);			
		SHA256_Final (m_H, &ctx);
		// h = SHA256(h || pub)
		SHA256_Init (&ctx);
		SHA256_Update (&ctx, m_H, 32);
		SHA256_Update (&ctx, pub, 32);
		SHA256_Final (m_H, &ctx);
		// x25519 between rs and priv
		uint8_t inputKeyMaterial[32];
		i2p::crypto::GetEd25519 ()->ScalarMul (rs, priv, inputKeyMaterial, m_Ctx); // rs*priv
		MixKey (inputKeyMaterial, m_K);
	}

	void NTCP2Establisher::KDF1Alice ()
	{
		KeyDerivationFunction1 (m_RemoteStaticKey, GetPriv (), GetPub ());
	}
	
	void NTCP2Establisher::KDF1Bob ()
	{
		KeyDerivationFunction1 (GetRemotePub (), i2p::context.GetNTCP2StaticPrivateKey (), GetRemotePub ()); 
	}

	void NTCP2Establisher::KeyDerivationFunction2 (const uint8_t * sessionRequest, size_t sessionRequestLen)
	{		
		SHA256_CTX ctx;
		SHA256_Init (&ctx);
		SHA256_Update (&ctx, m_H, 32);
		SHA256_Update (&ctx, sessionRequest + 32, 32); // encrypted payload	
		SHA256_Final (m_H, &ctx);

		int paddingLength =  sessionRequestLen - 64;
		if (paddingLength > 0)
		{
			SHA256_Init (&ctx);
			SHA256_Update (&ctx, m_H, 32);			
			SHA256_Update (&ctx, sessionRequest + 64, paddingLength);			
			SHA256_Final (m_H, &ctx);
		}	
		SHA256_Init (&ctx);
		SHA256_Update (&ctx, m_H, 32);
		SHA256_Update (&ctx, GetRemotePub (), 32); 
		SHA256_Final (m_H, &ctx);

		// x25519 between remote pub and priv
		uint8_t inputKeyMaterial[32];
		i2p::crypto::GetEd25519 ()->ScalarMul (GetRemotePub (), GetPriv (), inputKeyMaterial, m_Ctx); 
		MixKey (inputKeyMaterial, m_K);
	}

	void NTCP2Establisher::KDF3Alice ()
	{
		uint8_t inputKeyMaterial[32];
		i2p::crypto::GetEd25519 ()->ScalarMul (GetRemotePub (), i2p::context.GetNTCP2StaticPrivateKey (), inputKeyMaterial, m_Ctx); 
		MixKey (inputKeyMaterial, m_K);
	}

	void NTCP2Establisher::KDF3Bob ()
	{
		uint8_t inputKeyMaterial[32];
		i2p::crypto::GetEd25519 ()->ScalarMul (m_RemoteStaticKey, m_EphemeralPrivateKey, inputKeyMaterial, m_Ctx); 
		MixKey (inputKeyMaterial, m_K);
	}

	void NTCP2Establisher::CreateEphemeralKey ()
	{
		RAND_bytes (m_EphemeralPrivateKey, 32);
		i2p::crypto::GetEd25519 ()->ScalarMulB (m_EphemeralPrivateKey, m_EphemeralPublicKey, m_Ctx);
	}

	NTCP2Session::NTCP2Session (NTCP2Server& server, std::shared_ptr<const i2p::data::RouterInfo> in_RemoteRouter):
		TransportSession (in_RemoteRouter, 30), 
		m_Server (server), m_Socket (m_Server.GetService ()), 
		m_IsEstablished (false), m_IsTerminated (false),
		m_SessionRequestBuffer (nullptr), m_SessionCreatedBuffer (nullptr), m_SessionConfirmedBuffer (nullptr),
		m_NextReceivedBuffer (nullptr), m_NextSendBuffer (nullptr),
		m_ReceiveSequenceNumber (0), m_SendSequenceNumber (0), m_IsSending (false)
	{
		m_Establisher.reset (new NTCP2Establisher);
		auto addr = in_RemoteRouter->GetNTCPAddress ();
		if (addr->ntcp2)
		{
			memcpy (m_Establisher->m_RemoteStaticKey, addr->ntcp2->staticKey, 32);
			memcpy (m_Establisher->m_IV, addr->ntcp2->iv, 16);
		}
		else
			LogPrint (eLogWarning, "NTCP2: Missing NTCP2 parameters"); 
	}

	NTCP2Session::~NTCP2Session ()
	{
		delete[] m_SessionRequestBuffer; 
		delete[] m_SessionCreatedBuffer;
		delete[] m_SessionConfirmedBuffer;
		delete[] m_NextReceivedBuffer;
		delete[] m_NextSendBuffer;
	}

	void NTCP2Session::Terminate ()
	{
		if (!m_IsTerminated)
		{
			m_IsTerminated = true;
			m_IsEstablished = false;
			m_Socket.close ();
			transports.PeerDisconnected (shared_from_this ());
			m_Server.RemoveNTCP2Session (shared_from_this ());
			m_SendQueue.clear ();
			LogPrint (eLogDebug, "NTCP2: session terminated");
		}
	}

	void NTCP2Session::Done ()
	{
		m_Server.GetService ().post (std::bind (&NTCP2Session::Terminate, shared_from_this ()));
	}

	void NTCP2Session::Established ()
	{
		m_IsEstablished = true;
		m_Establisher.reset (nullptr);
		transports.PeerConnected (shared_from_this ());
	}

	void NTCP2Session::CreateNonce (uint64_t seqn, uint8_t * nonce)
	{
		memset (nonce, 0, 4); 
		htole64buf (nonce + 4, seqn); 
	}


	void NTCP2Session::KeyDerivationFunctionDataPhase ()
	{
		uint8_t tempKey[32]; unsigned int len;
		HMAC(EVP_sha256(), m_Establisher->GetCK (), 32, nullptr, 0, tempKey, &len); // temp_key = HMAC-SHA256(ck, zerolen)
		static uint8_t one[1] =  { 1 };
		HMAC(EVP_sha256(), tempKey, 32, one, 1, m_Kab, &len);  // k_ab = HMAC-SHA256(temp_key, byte(0x01)).
		m_Kab[32] = 2;
		HMAC(EVP_sha256(), tempKey, 32, m_Kab, 33, m_Kba, &len);  // k_ba = HMAC-SHA256(temp_key, k_ab || byte(0x02))
		static uint8_t ask[4] = { 'a', 's', 'k', 1 }, master[32];
		HMAC(EVP_sha256(), tempKey, 32, ask, 4, master, &len); // ask_master = HMAC-SHA256(temp_key, "ask" || byte(0x01))
		uint8_t h[39];
		memcpy (h, m_Establisher->GetH (), 32);
		memcpy (h + 32, "siphash", 7);
		HMAC(EVP_sha256(), master, 32, h, 39, tempKey, &len); // temp_key = HMAC-SHA256(ask_master, h || "siphash")
		HMAC(EVP_sha256(), tempKey, 32, one, 1, master, &len); // sip_master = HMAC-SHA256(temp_key, byte(0x01))  
		HMAC(EVP_sha256(), master, 32, nullptr, 0, tempKey, &len); // temp_key = HMAC-SHA256(sip_master, zerolen)
		HMAC(EVP_sha256(), tempKey, 32, one, 1, m_Sipkeysab, &len); // sipkeys_ab = HMAC-SHA256(temp_key, byte(0x01)).
		m_Sipkeysab[32] = 2;
		HMAC(EVP_sha256(), tempKey, 32, m_Sipkeysab, 33, m_Sipkeysba, &len); // sipkeys_ba = HMAC-SHA256(temp_key, sipkeys_ab || byte(0x02)) 
	}


	void NTCP2Session::SendSessionRequest ()
	{
		// create buffer and fill padding
		auto paddingLength = rand () % (287 - 64); // message length doesn't exceed 287 bytes
		m_SessionRequestBufferLen = paddingLength + 64;
		m_SessionRequestBuffer = new uint8_t[m_SessionRequestBufferLen];
		RAND_bytes (m_SessionRequestBuffer + 64, paddingLength);
		// encrypt X
		i2p::crypto::CBCEncryption encryption;
		encryption.SetKey (GetRemoteIdentity ()->GetIdentHash ());
		encryption.SetIV (m_Establisher->m_IV);
		encryption.Encrypt (m_Establisher->GetPub (), 32, m_SessionRequestBuffer); // X
		encryption.GetIV (m_Establisher->m_IV); // save IV for SessionCreated	
		// encryption key for next block
		m_Establisher->KDF1Alice ();
		// fill options
		uint8_t options[32]; // actual options size is 16 bytes
		memset (options, 0, 16);
		options[1] = 2; // ver	
		htobe16buf (options + 2, paddingLength); // padLen
		m_Establisher->m3p2Len = i2p::context.GetRouterInfo ().GetBufferLen () + 20; // (RI header + RI + MAC for now) TODO: implement options
		htobe16buf (options + 4,  m_Establisher->m3p2Len);
		// 2 bytes reserved
		htobe32buf (options + 8, i2p::util::GetSecondsSinceEpoch ()); // tsA
		// 4 bytes reserved
		// sign and encrypt options, use m_H as AD			
		uint8_t nonce[12];
		memset (nonce, 0, 12); // set nonce to zero
		i2p::crypto::AEADChaCha20Poly1305 (options, 16, m_Establisher->GetH (), 32, m_Establisher->GetK (), nonce, m_SessionRequestBuffer + 32, 32, true); // encrypt
		// send message
		boost::asio::async_write (m_Socket, boost::asio::buffer (m_SessionRequestBuffer, m_SessionRequestBufferLen), boost::asio::transfer_all (),
			std::bind(&NTCP2Session::HandleSessionRequestSent, shared_from_this (), std::placeholders::_1, std::placeholders::_2));		
	}	

	void NTCP2Session::HandleSessionRequestSent (const boost::system::error_code& ecode, std::size_t bytes_transferred)
	{
		(void) bytes_transferred;
		if (ecode)
		{
			LogPrint (eLogWarning, "NTCP2: couldn't send SessionRequest message: ", ecode.message ());
			Terminate ();
		}
		else
		{
			m_SessionCreatedBuffer = new uint8_t[287]; // TODO: determine actual max size
			// we receive first 64 bytes (32 Y, and 32 ChaCha/Poly frame) first
			boost::asio::async_read (m_Socket, boost::asio::buffer(m_SessionCreatedBuffer, 64), boost::asio::transfer_all (),
				std::bind(&NTCP2Session::HandleSessionCreatedReceived, shared_from_this (), std::placeholders::_1, std::placeholders::_2));
		}
	}

	void NTCP2Session::HandleSessionRequestReceived (const boost::system::error_code& ecode, std::size_t bytes_transferred)
	{
		(void) bytes_transferred;
		if (ecode)
		{
			LogPrint (eLogWarning, "NTCP2: SessionRequest read error: ", ecode.message ());
			Terminate ();
		}
		else
		{
			// decrypt X
			i2p::crypto::CBCDecryption decryption;
			decryption.SetKey (i2p::context.GetIdentHash ());
			decryption.SetIV (i2p::context.GetNTCP2IV ());
			decryption.Decrypt (m_SessionRequestBuffer, 32, m_Establisher->GetRemotePub ());
			decryption.GetIV (m_Establisher->m_IV); // save IV for SessionCreated	
			// decryption key for next block
			m_Establisher->KDF1Bob ();
			// verify MAC and decrypt options block (32 bytes), use m_H as AD
			uint8_t nonce[12], options[16];
			memset (nonce, 0, 12); // set nonce to zero
			if (i2p::crypto::AEADChaCha20Poly1305 (m_SessionRequestBuffer + 32, 16, m_Establisher->GetH (), 32, m_Establisher->GetK (), nonce, options, 16, false)) // decrypt
			{
				if (options[1] == 2)
				{
					uint16_t paddingLen = bufbe16toh (options + 2);
					m_SessionRequestBufferLen = paddingLen + 64;
					m_Establisher->m3p2Len = bufbe16toh (options + 4);
					// TODO: check tsA
					if (paddingLen > 0)
						boost::asio::async_read (m_Socket, boost::asio::buffer(m_SessionRequestBuffer + 64, paddingLen), boost::asio::transfer_all (),
						std::bind(&NTCP2Session::HandleSessionRequestPaddingReceived, shared_from_this (), std::placeholders::_1, std::placeholders::_2));
					else
						SendSessionCreated ();
				}
				else
				{
					LogPrint (eLogWarning, "NTCP2: SessionRequest version mismatch ", (int)options[1]);
					Terminate ();
				}
			}
			else
			{
				LogPrint (eLogWarning, "NTCP2: SessionRequest AEAD verification failed ");
				Terminate ();
			}	
		}
	}

	void NTCP2Session::HandleSessionRequestPaddingReceived (const boost::system::error_code& ecode, std::size_t bytes_transferred)
	{
		if (ecode)
		{
			LogPrint (eLogWarning, "NTCP2: SessionRequest padding read error: ", ecode.message ());
			Terminate ();
		}
		else
			SendSessionCreated ();
	}

	void NTCP2Session::SendSessionCreated ()
	{
		m_SessionCreatedBuffer = new uint8_t[287]; // TODO: determine actual max size
		// encrypt Y
		i2p::crypto::CBCEncryption encryption;
		encryption.SetKey (i2p::context.GetIdentHash ());
		encryption.SetIV (m_Establisher->m_IV);
		encryption.Encrypt (m_Establisher->GetPub (), 32, m_SessionCreatedBuffer); // Y
		// encryption key for next block (m_K)
		m_Establisher->KeyDerivationFunction2 (m_SessionRequestBuffer, m_SessionRequestBufferLen);	
		auto paddingLen = rand () % (287 - 64);
		uint8_t options[16];
		memset (options, 0, 16);
		htobe16buf (options + 2, paddingLen); // padLen
		htobe32buf (options + 8, i2p::util::GetSecondsSinceEpoch ()); // tsB
		// sign and encrypt options, use m_H as AD			
		uint8_t nonce[12];
		memset (nonce, 0, 12); // set nonce to zero
		i2p::crypto::AEADChaCha20Poly1305 (options, 16, m_Establisher->GetH (), 32, m_Establisher->GetK (), nonce, m_SessionCreatedBuffer + 32, 32, true); // encrypt
		// fill padding
		RAND_bytes (m_SessionCreatedBuffer + 56, paddingLen);
		// send message		
		m_SessionCreatedBufferLen = paddingLen + 64;
		boost::asio::async_write (m_Socket, boost::asio::buffer (m_SessionCreatedBuffer, m_SessionCreatedBufferLen), boost::asio::transfer_all (),
			std::bind(&NTCP2Session::HandleSessionCreatedSent, shared_from_this (), std::placeholders::_1, std::placeholders::_2));	
	}

	void NTCP2Session::HandleSessionCreatedReceived (const boost::system::error_code& ecode, std::size_t bytes_transferred)
	{
		if (ecode)
		{
			LogPrint (eLogWarning, "NTCP2: SessionCreated read error: ", ecode.message ());
			Terminate ();
		}
		else
		{
			LogPrint (eLogDebug, "NTCP2: SessionCreated received ", bytes_transferred);
			m_SessionCreatedBufferLen = 64;
			// decrypt Y
			i2p::crypto::CBCDecryption decryption;
			decryption.SetKey (GetRemoteIdentity ()->GetIdentHash ());
			decryption.SetIV (m_Establisher->m_IV);
			decryption.Decrypt (m_SessionCreatedBuffer, 32, m_Establisher->GetRemotePub ());
			// decryption key for next block (m_K)
			m_Establisher->KeyDerivationFunction2 (m_SessionRequestBuffer, m_SessionRequestBufferLen);
			// decrypt and verify MAC
			uint8_t payload[16];
			uint8_t nonce[12];
			memset (nonce, 0, 12); // set nonce to zero
			if (i2p::crypto::AEADChaCha20Poly1305 (m_SessionCreatedBuffer + 32, 16, m_Establisher->GetH (), 32, m_Establisher->GetK (), nonce, payload, 16, false)) // decrypt
			{		
				uint16_t paddingLen = bufbe16toh(payload + 2);
				LogPrint (eLogDebug, "NTCP2: padding length ", paddingLen);
				// TODO: check tsB
				if (paddingLen > 0)
				{
					boost::asio::async_read (m_Socket, boost::asio::buffer(m_SessionCreatedBuffer + 64, paddingLen), boost::asio::transfer_all (),
						std::bind(&NTCP2Session::HandleSessionCreatedPaddingReceived, shared_from_this (), std::placeholders::_1, std::placeholders::_2));
				}
				else
					SendSessionConfirmed ();
			}
			else
			{	
				LogPrint (eLogWarning, "NTCP2: SessionCreated MAC verification failed ");
				Terminate ();
			}	
		}
	}

	void NTCP2Session::HandleSessionCreatedPaddingReceived (const boost::system::error_code& ecode, std::size_t bytes_transferred)
	{
		if (ecode)
		{
			LogPrint (eLogWarning, "NTCP2: SessionCreated padding read error: ", ecode.message ());
			Terminate ();
		}
		else
		{
			m_SessionCreatedBufferLen += bytes_transferred;
			SendSessionConfirmed ();
		}
	}


	void NTCP2Session::SendSessionConfirmed ()
	{
		// update AD
		uint8_t h[80];
		memcpy (h, m_Establisher->GetH (), 32);
		memcpy (h + 32, m_SessionCreatedBuffer + 32, 32); // encrypted payload
		SHA256 (h, 64, h); 
		int paddingLength = m_SessionCreatedBufferLen - 64;
		if (paddingLength > 0)
		{
			SHA256_CTX ctx;
			SHA256_Init (&ctx);
			SHA256_Update (&ctx, h, 32);			
			SHA256_Update (&ctx, m_SessionCreatedBuffer + 64, paddingLength);			
			SHA256_Final (h, &ctx);
		}	
		// part1 48 bytes 
		m_SessionConfirmedBuffer = new uint8_t[2048]; // TODO: actual size
		uint8_t nonce[12];
		CreateNonce (1, nonce);
		i2p::crypto::AEADChaCha20Poly1305 (i2p::context.GetNTCP2StaticPublicKey (), 32, h, 32, m_Establisher->GetK (), nonce, m_SessionConfirmedBuffer, 48, true); // encrypt
		// part 2
		// update AD again
		memcpy (h + 32, m_SessionConfirmedBuffer, 48);
		SHA256 (h, 80, m_Establisher->m_H); 			

		std::vector<uint8_t> buf(m_Establisher->m3p2Len - 16); // -MAC
		buf[0] = 2; // block
		htobe16buf (buf.data () + 1, i2p::context.GetRouterInfo ().GetBufferLen () + 1); // flag + RI
		buf[3] = 0; // flag 	
		memcpy (buf.data () + 4, i2p::context.GetRouterInfo ().GetBuffer (), i2p::context.GetRouterInfo ().GetBufferLen ());
		m_Establisher->KDF3Alice (); 
		memset (nonce, 0, 12); // set nonce to 0 again
		i2p::crypto::AEADChaCha20Poly1305 (buf.data (), m_Establisher->m3p2Len - 16, m_Establisher->GetH (), 32, m_Establisher->GetK (), nonce, m_SessionConfirmedBuffer + 48, m_Establisher->m3p2Len, true); // encrypt
		uint8_t tmp[48];
		memcpy (tmp, m_SessionConfirmedBuffer, 48);
		memcpy (m_SessionConfirmedBuffer + 16, m_Establisher->GetH (), 32); // h || ciphertext
		SHA256 (m_SessionConfirmedBuffer + 16, m_Establisher->m3p2Len + 32, m_Establisher->m_H); //h = SHA256(h || ciphertext);
		memcpy (m_SessionConfirmedBuffer, tmp, 48);	

		// send message
		boost::asio::async_write (m_Socket, boost::asio::buffer (m_SessionConfirmedBuffer, m_Establisher->m3p2Len + 48), boost::asio::transfer_all (),
			std::bind(&NTCP2Session::HandleSessionConfirmedSent, shared_from_this (), std::placeholders::_1, std::placeholders::_2));
	}

	void NTCP2Session::HandleSessionConfirmedSent (const boost::system::error_code& ecode, std::size_t bytes_transferred)
	{
		LogPrint (eLogDebug, "NTCP2: SessionConfirmed sent");
		KeyDerivationFunctionDataPhase ();
		// Alice
		m_SendKey = m_Kab;
		m_ReceiveKey = m_Kba; 
		m_SendSipKey = m_Sipkeysab; 
		m_ReceiveSipKey = m_Sipkeysba;
		memcpy (m_ReceiveIV, m_Sipkeysba + 16, 8);
		memcpy (m_SendIV, m_Sipkeysab + 16, 8);
		Established ();
		ReceiveLength ();

		// TODO: remove
		//m_SendQueue.push_back (CreateDeliveryStatusMsg (1));
		//SendQueue ();
	}

	void NTCP2Session::HandleSessionCreatedSent (const boost::system::error_code& ecode, std::size_t bytes_transferred)
	{
		(void) bytes_transferred;
		if (ecode)
		{
			LogPrint (eLogWarning, "NTCP2: couldn't send SessionCreated message: ", ecode.message ());
			Terminate ();
		}
		else
		{
			LogPrint (eLogDebug, "NTCP2: SessionCreated sent");
			m_SessionConfirmedBuffer = new uint8_t[m_Establisher->m3p2Len + 48]; 
			boost::asio::async_read (m_Socket, boost::asio::buffer(m_SessionConfirmedBuffer, m_Establisher->m3p2Len + 48), boost::asio::transfer_all (),
				std::bind(&NTCP2Session::HandleSessionConfirmedReceived , shared_from_this (), std::placeholders::_1, std::placeholders::_2));
		}
	}

	void NTCP2Session::HandleSessionConfirmedReceived (const boost::system::error_code& ecode, std::size_t bytes_transferred)
	{
		if (ecode)
		{
			LogPrint (eLogWarning, "NTCP2: SessionConfirmed Part 1 read error: ", ecode.message ());
			Terminate ();
		}
		else
		{
			LogPrint (eLogDebug, "NTCP2: SessionConfirmed Part 1 received");
			// update AD
			uint8_t h[80];
			memcpy (h, m_Establisher->GetH (), 32);
			memcpy (h + 32, m_SessionCreatedBuffer + 32, 32); // encrypted payload
			SHA256 (h, 64, h); 
			int paddingLength = m_SessionCreatedBufferLen - 64;
			if (paddingLength > 0)
			{
				SHA256_CTX ctx;
				SHA256_Init (&ctx);
				SHA256_Update (&ctx, h, 32);			
				SHA256_Update (&ctx, m_SessionCreatedBuffer + 64, paddingLength);			
				SHA256_Final (h, &ctx);
			}	
			// part 1
			uint8_t nonce[12];
			CreateNonce (1, nonce);
			i2p::crypto::AEADChaCha20Poly1305 (m_SessionConfirmedBuffer, 48, h, 32, m_Establisher->GetK (), nonce, m_Establisher->m_RemoteStaticKey, 32, false); // decrypt S
			// part 2
			// update AD again
			memcpy (h + 32, m_SessionConfirmedBuffer, 48);
			SHA256 (h, 80, m_Establisher->m_H); 	

			std::vector<uint8_t> buf(m_Establisher->m3p2Len - 16); // -MAC
			m_Establisher->KDF3Bob (); 
			memset (nonce, 0, 12); // set nonce to 0 again
			i2p::crypto::AEADChaCha20Poly1305 (m_SessionConfirmedBuffer + 48, m_Establisher->m3p2Len, m_Establisher->GetH (), 32, m_Establisher->GetK (), nonce, buf.data (), m_Establisher->m3p2Len - 16, false); // decrypt
			// process RI
			if (buf[0] == eNTCP2BlkRouterInfo)
			{
				auto size = bufbe16toh (buf.data () + 1);
				if (size <= buf.size () - 3)
				{
					// TODO: check flag
					i2p::data::netdb.AddRouterInfo (buf.data () + 4, size - 1); // 1 byte block type + 2 bytes size + 1 byte flag
					// TODO: process options
				}
				else
					LogPrint (eLogError, "NTCP2: Unexpected RouterInfo size ", size, " in SessionConfirmed");
			}
			else
				LogPrint (eLogWarning, "NTCP2: unexpected block ", (int)buf[0], " in SessionConfirmed");		
			// caclulate new h again for KDF data
			memcpy (m_SessionConfirmedBuffer + 16, m_Establisher->GetH (), 32); // h || ciphertext
			SHA256 (m_SessionConfirmedBuffer + 16, m_Establisher->m3p2Len + 32, m_Establisher->m_H); //h = SHA256(h || ciphertext);
			KeyDerivationFunctionDataPhase ();
			// Bob
			m_SendKey = m_Kba;
			m_ReceiveKey = m_Kab; 
			m_SendSipKey = m_Sipkeysba; 
			m_ReceiveSipKey = m_Sipkeysab;
			memcpy (m_ReceiveIV, m_Sipkeysab + 16, 8);
			memcpy (m_SendIV, m_Sipkeysba + 16, 8);
			Established ();
			ReceiveLength ();			
		}
	}

	void NTCP2Session::ClientLogin ()
	{
		SendSessionRequest ();
	}

	void NTCP2Session::ServerLogin ()
	{
		m_SessionRequestBuffer = new uint8_t[287]; // 287 bytes max for now
		boost::asio::async_read (m_Socket, boost::asio::buffer(m_SessionRequestBuffer, 64), boost::asio::transfer_all (),
			std::bind(&NTCP2Session::HandleSessionRequestReceived, shared_from_this (),
				std::placeholders::_1, std::placeholders::_2));
	}

	void NTCP2Session::ReceiveLength ()
	{
		boost::asio::async_read (m_Socket, boost::asio::buffer(&m_NextReceivedLen, 2), boost::asio::transfer_all (),
			std::bind(&NTCP2Session::HandleReceivedLength, shared_from_this (), std::placeholders::_1, std::placeholders::_2));
	}

	void NTCP2Session::HandleReceivedLength (const boost::system::error_code& ecode, std::size_t bytes_transferred)
	{
		if (ecode)
		{
			LogPrint (eLogWarning, "NTCP2: receive length read error: ", ecode.message ());
			Terminate ();
		}
		else
		{
			i2p::crypto::Siphash<8> (m_ReceiveIV, m_ReceiveIV, 8, m_ReceiveSipKey); 
			m_NextReceivedLen = be16toh (m_NextReceivedLen ^ bufbe16toh(m_ReceiveIV));
			LogPrint (eLogDebug, "NTCP2: received length ", m_NextReceivedLen);
			delete[] m_NextReceivedBuffer;
			m_NextReceivedBuffer = new uint8_t[m_NextReceivedLen];
			Receive ();
		}
	}

	void NTCP2Session::Receive ()
	{
		boost::asio::async_read (m_Socket, boost::asio::buffer(m_NextReceivedBuffer, m_NextReceivedLen), boost::asio::transfer_all (),
			std::bind(&NTCP2Session::HandleReceived, shared_from_this (), std::placeholders::_1, std::placeholders::_2));
	}

	void NTCP2Session::HandleReceived (const boost::system::error_code& ecode, std::size_t bytes_transferred)
	{
		if (ecode)
		{
			LogPrint (eLogWarning, "NTCP2: receive read error: ", ecode.message ());
			Terminate ();
		}
		else
		{
			m_NumReceivedBytes += bytes_transferred + 2; // + length
			i2p::transport::transports.UpdateReceivedBytes (bytes_transferred);
			uint8_t nonce[12];
			CreateNonce (m_ReceiveSequenceNumber, nonce); m_ReceiveSequenceNumber++;
			uint8_t * decrypted = new uint8_t[m_NextReceivedLen];
			if (i2p::crypto::AEADChaCha20Poly1305 (m_NextReceivedBuffer, m_NextReceivedLen-16, nullptr, 0, m_ReceiveKey, nonce, decrypted, m_NextReceivedLen, false))
			{	
				LogPrint (eLogDebug, "NTCP2: received message decrypted");
				ProcessNextFrame (decrypted, m_NextReceivedLen-16);
				ReceiveLength ();
			}
			else
			{
				LogPrint (eLogWarning, "NTCP2: Received MAC verification failed ");
				Terminate ();
			}	
			delete[] decrypted;
		}
	}

	void NTCP2Session::ProcessNextFrame (const uint8_t * frame, size_t len)
	{
		size_t offset = 0;
		while (offset < len)
		{
			uint8_t blk = frame[offset];
			offset++;
			auto size = bufbe16toh (frame + offset);
			offset += 2;
			LogPrint (eLogDebug, "NTCP2: Block type ", (int)blk, " of size ", size);
			if (size > len)
			{
				LogPrint (eLogError, "NTCP2: Unexpected block length ", size);
				break;
			}
			switch (blk)
			{
				case eNTCP2BlkDateTime:
					LogPrint (eLogDebug, "NTCP2: datetime");
				break;	
				case eNTCP2BlkOptions:
					LogPrint (eLogDebug, "NTCP2: options");
				break;
				case eNTCP2BlkRouterInfo:
				{
					LogPrint (eLogDebug, "NTCP2: RouterInfo flag=", (int)frame[offset]);
					i2p::data::netdb.AddRouterInfo (frame + offset + 1, size - 1);
					break;
				}
				case eNTCP2BlkI2NPMessage:
				{
					LogPrint (eLogDebug, "NTCP2: I2NP");
					auto nextMsg = NewI2NPMessage (size);
					nextMsg->len = nextMsg->offset + size + 7; // 7 more bytes for full I2NP header
					memcpy (nextMsg->GetNTCP2Header (), frame + offset, size);
					nextMsg->FromNTCP2 ();
					m_Handler.PutNextMessage (nextMsg);	
					break;
				}
				case eNTCP2BlkTermination:
					LogPrint (eLogDebug, "NTCP2: termination");
				break;
				case eNTCP2BlkPadding:
					LogPrint (eLogDebug, "NTCP2: padding");
				break;
				default:
					LogPrint (eLogWarning, "NTCP2: Unknown block type ", (int)blk);
			}
			offset += size;
		}
		m_Handler.Flush ();
	}

	void NTCP2Session::SendNextFrame (const uint8_t * payload, size_t len)
	{
		uint8_t nonce[12];
		CreateNonce (m_SendSequenceNumber, nonce); m_SendSequenceNumber++;
		m_NextSendBuffer = new uint8_t[len + 16 + 2];
		i2p::crypto::AEADChaCha20Poly1305 (payload, len, nullptr, 0, m_SendKey, nonce, m_NextSendBuffer + 2, len + 16, true);
		i2p::crypto::Siphash<8> (m_SendIV, m_SendIV, 8, m_SendSipKey);
		htobuf16 (m_NextSendBuffer, bufbe16toh (m_SendIV) ^ htobe16(len + 16));
		LogPrint (eLogDebug, "NTCP2: sent length ", len + 16);

		// send message
		m_IsSending = true;	
		boost::asio::async_write (m_Socket, boost::asio::buffer (m_NextSendBuffer, len + 16 + 2), boost::asio::transfer_all (),
			std::bind(&NTCP2Session::HandleNextFrameSent, shared_from_this (), std::placeholders::_1, std::placeholders::_2));
	}

	void NTCP2Session::HandleNextFrameSent (const boost::system::error_code& ecode, std::size_t bytes_transferred)
	{
		m_NumSentBytes += bytes_transferred;
		i2p::transport::transports.UpdateSentBytes (bytes_transferred);
		delete[] m_NextSendBuffer; m_NextSendBuffer = nullptr;
		LogPrint (eLogDebug, "NTCP2: Next frame sent");
		m_IsSending = false;
		SendQueue ();
	}

	void NTCP2Session::SendQueue ()
	{
		if (!m_SendQueue.empty ())
		{
			uint8_t * payload = new uint8_t[NTCP2_UNENCRYPTED_FRAME_MAX_SIZE];
			size_t s = 0;
			// add I2NP blocks
			while (!m_SendQueue.empty ())
			{
				auto msg = m_SendQueue.front ();
				size_t len = msg->GetNTCP2Length (); 
				if (s + len + 3 <= NTCP2_UNENCRYPTED_FRAME_MAX_SIZE) // 3 bytes block header
				{
					payload[s] = eNTCP2BlkI2NPMessage; // blk
					htobe16buf (payload + s + 1, len); // size
					s += 3;
					msg->ToNTCP2 ();
					memcpy (payload + s, msg->GetNTCP2Header (), len);
					s += len;
					m_SendQueue.pop_front ();
				}
				else
					break;
			}
			// add padding block 
			int paddingSize = (s*NTCP2_MAX_PADDING_RATIO)/100;
			if (s + paddingSize + 3 > NTCP2_UNENCRYPTED_FRAME_MAX_SIZE) paddingSize = NTCP2_UNENCRYPTED_FRAME_MAX_SIZE - s -3;
			if (paddingSize) paddingSize = rand () % paddingSize;
			payload[s] = eNTCP2BlkPadding; // blk
			htobe16buf (payload + s + 1, paddingSize); // size
			s += 3;
			RAND_bytes (payload + s, paddingSize);			
			s += paddingSize;
			// send
			SendNextFrame (payload, s);
			delete[] payload;
		} 
	}

	void NTCP2Session::SendI2NPMessages (const std::vector<std::shared_ptr<I2NPMessage> >& msgs)
	{
		m_Server.GetService ().post (std::bind (&NTCP2Session::PostI2NPMessages, shared_from_this (), msgs));
	}

	void NTCP2Session::PostI2NPMessages (std::vector<std::shared_ptr<I2NPMessage> > msgs)
	{
		if (m_IsTerminated) return;
		for (auto it: msgs)
			m_SendQueue.push_back (it);
		if (!m_IsSending) 
			SendQueue ();		
	}

	NTCP2Server::NTCP2Server ():
		m_IsRunning (false), m_Thread (nullptr), m_Work (m_Service)	
	{
	}

	NTCP2Server::~NTCP2Server ()
	{
		Stop ();
	}

	void NTCP2Server::Start ()
	{
		if (!m_IsRunning)
		{
			m_IsRunning = true;
			m_Thread = new std::thread (std::bind (&NTCP2Server::Run, this));
			auto& addresses = context.GetRouterInfo ().GetAddresses ();
			for (const auto& address: addresses)
			{
				if (!address) continue;
				if (address->IsPublishedNTCP2 ())
				{
					if (address->host.is_v4())
					{
						try
						{
							m_NTCP2Acceptor.reset (new boost::asio::ip::tcp::acceptor (m_Service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), address->port)));
						} 
						catch ( std::exception & ex ) 
						{
							LogPrint(eLogError, "NTCP2: Failed to bind to ip4 port ",address->port, ex.what());
							continue;
						}

						LogPrint (eLogInfo, "NTC2P: Start listening TCP port ", address->port);
						auto conn = std::make_shared<NTCP2Session>(*this);
						m_NTCP2Acceptor->async_accept(conn->GetSocket (), std::bind (&NTCP2Server::HandleAccept, this, conn, std::placeholders::_1));
					}
					else if (address->host.is_v6() && context.SupportsV6 ())
					{
						m_NTCP2V6Acceptor.reset (new boost::asio::ip::tcp::acceptor (m_Service));
						try
						{
							m_NTCP2V6Acceptor->open (boost::asio::ip::tcp::v6());
							m_NTCP2V6Acceptor->set_option (boost::asio::ip::v6_only (true));
							m_NTCP2V6Acceptor->bind (boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), address->port));
							m_NTCP2V6Acceptor->listen ();

							LogPrint (eLogInfo, "NTCP2: Start listening V6 TCP port ", address->port);
							auto conn = std::make_shared<NTCP2Session> (*this);
							m_NTCP2V6Acceptor->async_accept(conn->GetSocket (), std::bind (&NTCP2Server::HandleAcceptV6, this, conn, std::placeholders::_1));
						} catch ( std::exception & ex ) {
							LogPrint(eLogError, "NTCP: failed to bind to ip6 port ", address->port);
							continue;
						}
					}
				}
			}
		}
	}

	void NTCP2Server::Stop ()
	{
		{
			// we have to copy it because Terminate changes m_NTCP2Sessions
			auto ntcpSessions = m_NTCP2Sessions;
			for (auto& it: ntcpSessions)
				it.second->Terminate ();
			for (auto& it: m_PendingIncomingSessions)
				it->Terminate ();
		}
		m_NTCP2Sessions.clear ();

		if (m_IsRunning)
		{
			m_IsRunning = false;
			m_Service.stop ();
			if (m_Thread)
			{
				m_Thread->join ();
				delete m_Thread;
				m_Thread = nullptr;
			}
		}
	}

	void NTCP2Server::Run ()
	{
		while (m_IsRunning)
		{
			try
			{
				m_Service.run ();
			}
			catch (std::exception& ex)
			{
				LogPrint (eLogError, "NTCP2: runtime exception: ", ex.what ());
			}
		}
	}

	bool NTCP2Server::AddNTCP2Session (std::shared_ptr<NTCP2Session> session)
	{
		if (!session || !session->GetRemoteIdentity ()) return false;
		auto& ident = session->GetRemoteIdentity ()->GetIdentHash ();
		auto it = m_NTCP2Sessions.find (ident);
		if (it != m_NTCP2Sessions.end ())
		{
			LogPrint (eLogWarning, "NTCP2: session to ", ident.ToBase64 (), " already exists");
			session->Terminate();
			return false;
		}
		m_NTCP2Sessions.insert (std::make_pair (ident, session));
		return true;
	}

	void NTCP2Server::RemoveNTCP2Session (std::shared_ptr<NTCP2Session> session)
	{
		if (session && session->GetRemoteIdentity ())
			m_NTCP2Sessions.erase (session->GetRemoteIdentity ()->GetIdentHash ());
	}

	std::shared_ptr<NTCP2Session> NTCP2Server::FindNTCP2Session (const i2p::data::IdentHash& ident)
	{
		auto it = m_NTCP2Sessions.find (ident);
		if (it != m_NTCP2Sessions.end ())
			return it->second;
		return nullptr;
	}

	void NTCP2Server::Connect(const boost::asio::ip::address & address, uint16_t port, std::shared_ptr<NTCP2Session> conn)
	{
		LogPrint (eLogDebug, "NTCP2: Connecting to ", address ,":",  port);
		m_Service.post([this, address, port, conn]() 
			{
				if (this->AddNTCP2Session (conn))
				{
					conn->GetSocket ().async_connect (boost::asio::ip::tcp::endpoint (address, port), std::bind (&NTCP2Server::HandleConnect, this, std::placeholders::_1, conn));
				}
			});
	}

	void NTCP2Server::HandleConnect (const boost::system::error_code& ecode, std::shared_ptr<NTCP2Session> conn)
	{
		if (ecode)
		{
			LogPrint (eLogInfo, "NTCP2: Connect error ", ecode.message ());
			conn->Terminate ();
		}
		else
		{
			LogPrint (eLogDebug, "NTCP2: Connected to ", conn->GetSocket ().remote_endpoint ());
			conn->ClientLogin ();
		}
	}

	void NTCP2Server::HandleAccept (std::shared_ptr<NTCP2Session> conn, const boost::system::error_code& error)
	{
		if (!error)
		{
			boost::system::error_code ec;
			auto ep = conn->GetSocket ().remote_endpoint(ec);
			if (!ec)
			{
				LogPrint (eLogDebug, "NTCP2: Connected from ", ep);
				if (conn)
				{
					conn->ServerLogin ();
				//	m_PendingIncomingSessions.push_back (conn);
				}
			}
			else
				LogPrint (eLogError, "NTCP2: Connected from error ", ec.message ());
		}

		if (error != boost::asio::error::operation_aborted)
		{
			conn = std::make_shared<NTCP2Session> (*this);
			m_NTCP2Acceptor->async_accept(conn->GetSocket (), std::bind (&NTCP2Server::HandleAccept, this,
				conn, std::placeholders::_1));
		}
	}

	void NTCP2Server::HandleAcceptV6 (std::shared_ptr<NTCP2Session> conn, const boost::system::error_code& error)
	{
		if (!error)
		{
			boost::system::error_code ec;
			auto ep = conn->GetSocket ().remote_endpoint(ec);
			if (!ec)
			{
				LogPrint (eLogDebug, "NTCP2: Connected from ", ep);
				if (conn)
				{
					conn->ServerLogin ();
				//	m_PendingIncomingSessions.push_back (conn);
				}
			}
			else
				LogPrint (eLogError, "NTCP2: Connected from error ", ec.message ());
		}

		if (error != boost::asio::error::operation_aborted)
		{
			conn = std::make_shared<NTCP2Session> (*this);
			m_NTCP2V6Acceptor->async_accept(conn->GetSocket (), std::bind (&NTCP2Server::HandleAcceptV6, this,
				conn, std::placeholders::_1));
		}
	}
}
}

