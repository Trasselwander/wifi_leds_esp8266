#include "CaptivePortal.h"
#include <lwip/def.h>
#include <Arduino.h>

CaptivePortal::CaptivePortal()
{
  _ttl = htonl(60);
  _errorReplyCode = DNSReplyCode::NonExistentDomain;
}

bool CaptivePortal::start(const uint16_t &port, const IPAddress &resolvedIP)
{
  _port = port;
  _buffer = NULL;
  _resolvedIP[0] = resolvedIP[0];
  _resolvedIP[1] = resolvedIP[1];
  _resolvedIP[2] = resolvedIP[2];
  _resolvedIP[3] = resolvedIP[3];
  return _udp.begin(_port) == 1;
}

void CaptivePortal::setErrorReplyCode(const DNSReplyCode &replyCode)
{
  _errorReplyCode = replyCode;
}

void CaptivePortal::setTTL(const uint32_t &ttl)
{
  _ttl = htonl(ttl);
}

void CaptivePortal::stop()
{
  _udp.stop();
  free(_buffer);
  _buffer = NULL;
}

void CaptivePortal::processNextRequest()
{
  _currentPacketSize = _udp.parsePacket();
  if (_currentPacketSize)
  {
    if (_buffer != NULL) free(_buffer);
    _buffer = (unsigned char*)malloc(_currentPacketSize * sizeof(char));
    if (_buffer == NULL) return;
    _udp.read(_buffer, _currentPacketSize);
    _dnsHeader = (DNSHeader*) _buffer;

    if (_dnsHeader->QR == DNS_QR_QUERY && _dnsHeader->OPCode == DNS_OPCODE_QUERY && requestIncludesOnlyOneQuestion())
      replyWithIP();
      
    free(_buffer);
    _buffer = NULL;
  }
}

bool CaptivePortal::requestIncludesOnlyOneQuestion()
{
  return ntohs(_dnsHeader->QDCount) == 1 &&
         _dnsHeader->ANCount == 0 &&
         _dnsHeader->NSCount == 0 &&
         _dnsHeader->ARCount == 0;
}

void CaptivePortal::replyWithIP()
{
  if (_buffer == NULL) return;
  _dnsHeader->QR = DNS_QR_RESPONSE;
  _dnsHeader->ANCount = _dnsHeader->QDCount;
  _dnsHeader->QDCount = _dnsHeader->QDCount;
  //_dnsHeader->RA = 1;

  _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
  _udp.write(_buffer, _currentPacketSize);

  _udp.write((uint8_t)192); //  answer name is a pointer
  _udp.write((uint8_t)12);  // pointer to offset at 0x00c

  _udp.write((uint8_t)0);   // 0x0001  answer is type A query (host address)
  _udp.write((uint8_t)1);

  _udp.write((uint8_t)0);   //0x0001 answer is class IN (internet address)
  _udp.write((uint8_t)1);

  _udp.write((unsigned char*)&_ttl, 4);

  // Length of RData is 4 bytes (because, in this case, RData is IPv4)
  _udp.write((uint8_t)0);
  _udp.write((uint8_t)4);
  _udp.write(_resolvedIP, sizeof(_resolvedIP));
  _udp.endPacket();
}
