#include "CloudClient.h"
#include "WiFiUdp.h"
#include <stdexcept>
#include <iostream>
uint32_t hostMillis=100;
FakeWifi WiFi;
FakeSerial Serial;
FakeUdpState udpState;
WireState wire;
namespace wol {
struct CloudClientTestAccess {
  static void init(CloudClient& c) { c.initializeTransport(); }
  static void tick(CloudClient& c) { c.tick(); }
};
}
// Run the actual production setup/loop and all application modules.
#include "../src/main.cpp"
static unsigned checks=0;
#define CHECK(x) do { ++checks; if(!(x)) throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x); } while(0)
using Bytes=std::vector<uint8_t>;
Bytes publish(const std::string& payload,const std::string& topic="testOnly006",uint8_t flags=0) {
  Bytes body={uint8_t(topic.size()>>8),uint8_t(topic.size())};
  body.insert(body.end(),topic.begin(),topic.end());
  if(flags&6) { body.push_back(0);body.push_back(1); }
  body.insert(body.end(),payload.begin(),payload.end());
  Bytes result={uint8_t(0x30|flags)};
  size_t n=body.size();do{uint8_t b=n%128;n/=128;if(n)b|=128;result.push_back(b);}while(n);
  result.insert(result.end(),body.begin(),body.end());return result;
}
void broker(int connack=0,int suback=0,bool pong=true,bool preHistory=false,bool sessionPresent=false) {
  wire.respond=[=](const Bytes& b){
    if(b.empty())return;
    if(b[0]==0x10) wire.feed({0x20,2,uint8_t(sessionPresent),uint8_t(connack)});
    if(b[0]==0x82) {
      if(preHistory) { wire.feed(publish("off"));wire.feed(publish("on")); }
      wire.feed({0x90,3,b[2],b[3],uint8_t(suback)});
    }
    if(b[0]==0xc0 && pong) wire.feed({0xd0,0});
  };
}
void pump(int steps=1,bool application=true) {
  for(int i=0;i<steps;++i) {
    wol::CloudClientTestAccess::tick(cloud);
    if(application) loop();
    ++hostMillis;
  }
}
void start() { setup(); CHECK(enabled);wol::CloudClientTestAccess::init(cloud);pump(); }
void ready() { CHECK(cloud.snapshot().online);hostMillis+=3100;pump(); }
void send(const std::string& s,uint8_t flags=0) { wire.feed(publish(s,"testOnly006",flags));pump(); }
void finishBurst() { for(int i=0;i<3;++i){hostMillis+=100;loop();} }
void wake() {send("off");send("on");finishBurst();}
void reconnect() {wire.up=false;pump();hostMillis+=61000;broker();pump();CHECK(cloud.snapshot().online);ready();}
void assertNoSecrets() {
  CHECK(Serial.text.find(wol::kSettings.uid)==std::string::npos);
  CHECK(Serial.text.find(wol::kSettings.password)==std::string::npos);
}
int main(int argc,char** argv) {
  try {
    CHECK(argc==2);const std::string scenario=argv[1];broker();
    if(scenario=="auth_rejected") broker(5);
    if(scenario=="session_present") broker(0,0,true,false,true);
    if(scenario=="suback_rejected") broker(0,0x80);
    if(scenario=="suback_qos1") broker(0,1);
    if(scenario=="pre_suback_history") broker(0,0,true,true);
    if(scenario=="connect_timeout") wire.respond=[](const Bytes&){};
    if(scenario=="tls_failure_no_downgrade") wire.connectOk=false;
    if(scenario=="wifi_offline") WiFi.state=0;
    start();
    if(scenario=="happy" || scenario=="session_present") {
      CHECK(wire.port==9503 && wire.tlsCaSet);CHECK(wire.written.size()>=2);
      const auto& c=wire.written[0];CHECK(c[0]==0x10 && c[8]==4 && c[9]==2);
      CHECK(c[12]==0 && c[13]==32);
      CHECK(std::string(c.begin()+14,c.end())==wol::kSettings.uid);
      const auto& s=wire.written[1];CHECK(s[0]==0x82 && s.back()==0);
      ready();wake();CHECK(udpState.ends==3);CHECK(udpState.payload.size()==102);
    } else if(scenario=="auth_rejected" || scenario=="suback_rejected" || scenario=="suback_qos1" || scenario=="connect_timeout") {
      CHECK(!cloud.snapshot().online);CHECK(udpState.ends==0);
      int n=wire.connects;pump(20);CHECK(wire.connects==n);
      hostMillis+=61000;broker();pump();ready();wake();CHECK(udpState.ends==3);
    } else if(scenario=="tls_failure_no_downgrade") {
      CHECK(!cloud.snapshot().online);CHECK(wire.port==9503);CHECK(wire.written.empty());
      hostMillis+=61000;pump();CHECK(wire.port==9503 && wire.connects==2);
    } else if(scenario=="wifi_offline") {
      CHECK(wire.connects==0);WiFi.state=WL_CONNECTED;pump();ready();wake();CHECK(udpState.ends==3);
    } else if(scenario=="pre_suback_history") {
      CHECK(cloud.snapshot().online);CHECK(udpState.ends==0);ready();send("on");CHECK(udpState.ends==0);wake();CHECK(udpState.ends==3);
    } else if(scenario=="guard") {
      send("off");send("on");CHECK(udpState.ends==0);ready();send("on");CHECK(udpState.ends==0);wake();CHECK(udpState.ends==3);
    } else if(scenario=="retained" || scenario=="dup") {
      ready();uint8_t f=scenario=="retained"?1:8;
      send("off",f);send("on");CHECK(udpState.ends==0);
      send("off");send("on",f);CHECK(udpState.ends==0);send("on");CHECK(udpState.ends==0);
      wake();CHECK(udpState.ends==3);
    } else if(scenario=="fragmented") {
      ready();wire.feed(publish("off"),7);pump(200);wire.feed(publish("on"),9);pump(200);finishBurst();CHECK(udpState.ends==3);
    } else if(scenario=="coalesced") {
      ready();wire.feed(publish("off"));wire.feed(publish("on"));pump(3);finishBurst();CHECK(udpState.ends==3);
    } else if(scenario=="queue_overflow") {
      ready();for(int i=0;i<12;++i)wire.feed(publish(i%2?"on":"off"));
      pump(15,false);CHECK(!cloud.snapshot().online);loop();CHECK(udpState.ends==0);
      hostMillis+=61000;pump();ready();send("on");CHECK(udpState.ends==0);wake();CHECK(udpState.ends==3);
    } else if(scenario=="stale_queue") {
      ready();wire.feed(publish("off"));wire.feed(publish("on"));pump(2,false);
      hostMillis+=2001;loop();CHECK(udpState.ends==0);send("on");CHECK(udpState.ends==0);wake();CHECK(udpState.ends==3);
    } else if(scenario=="disconnect_queued") {
      ready();wire.feed(publish("off"));wire.feed(publish("on"));pump(2,false);
      reconnect();CHECK(udpState.ends==0);send("on");CHECK(udpState.ends==0);wake();CHECK(udpState.ends==3);
    } else if(scenario=="cooldown") {
      ready();wake();CHECK(udpState.ends==3);wake();CHECK(udpState.ends==3);
      hostMillis+=15000;wake();CHECK(udpState.ends==6);
    } else if(scenario=="heartbeat_timeout") {
      ready();broker(0,0,false);
      for(int i=0;i<65 && cloud.snapshot().online;++i) { hostMillis+=1000;pump(); }
      CHECK(!cloud.snapshot().online);
      hostMillis+=61000;broker();pump();ready();wake();CHECK(udpState.ends==3);
    } else if(scenario=="partial_payload") {
      ready();auto b=publish("off");b.pop_back();wire.feed(b);pump();CHECK(!cloud.snapshot().online);CHECK(udpState.ends==0);
    } else if(scenario=="invalid_topic" || scenario=="empty" || scenario=="oversized" || scenario=="qos1") {
      ready();wire.feed(scenario=="invalid_topic"?publish("on","wrong006"):
        scenario=="empty"?publish(""):scenario=="qos1"?publish("on","testOnly006",2):publish(std::string(600,'x')));
      pump(3);CHECK(!cloud.snapshot().online);CHECK(udpState.ends==0);
    } else if(scenario=="reconnect_cycles") {
      ready();for(int i=0;i<1000;++i) {
        reconnect();send("on");CHECK(udpState.ends==3*i);wake();CHECK(udpState.ends==3*(i+1));
        wire.written.clear(); // Keep logs so the final secret check covers every cycle.
      }
    } else throw std::runtime_error("Unknown scenario");
    assertNoSecrets();std::cout<<"PASS "<<scenario<<" ("<<checks<<" checks)\n";return 0;
  } catch(const std::exception& e) {std::cerr<<"FAIL "<<argv[1]<<": "<<e.what()<<"\n"<<Serial.text;return 1;}
}
