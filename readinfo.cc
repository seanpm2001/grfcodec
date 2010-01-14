/*
 * readinfo.cc 
 * Reads an NFO file into an array of parsed sprites.
 * 
 * Bastard child of NFORenum's renum.cpp, pseudo.cpp, and inlines.h
 *
 * Copyright 2004-2006 by Dale McCoy.
 * dalestan@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
INFO VERSION HISTORY:
Version 1: First version
Version 2: Add sprite numbers, // (Info version x) line
Version 3: Change real-sprite format
Version 4: Fix bug in real-sprite format
Version 5: Add literal strings
Version 6: Add binary includes
Version 7: Add backslash escapes
*/

#include<cstring>
#include<climits>
#include<iostream>
#include<string>
#include<sstream>
#include<iomanip>
#include<cstdio>

using namespace std;

#ifndef NO_BOOST
// grfcodec requires boost::date_time for its processing of the \wYMD and
// \wDMY formats. Get boost from www.boost.org
// If you are not capable of downloading or installing boost,
// #define NO_BOOST before compiling grfcodec.
#include<boost/date_time/gregorian/gregorian_types.hpp>
using namespace boost::gregorian;

#endif//NO_BOOST

#include"nfosprite.h"
#include"allocarray.h"

extern int _quiet;

istream&eat_white(istream&in){
	while(isspace(in.peek()))in.ignore();
	return in;
}

int ctoi(char ch){
	if(ch>='0'&&ch<='9')return ch-'0';
	if(ch>='A'&&ch<='F')return ch-'A'+10;
	if(ch>='a'&&ch<='f')return ch-'a'+10;
	return 0;
}

string UCase(string str){
	size_t len=str.length();
	for(size_t i=0;i<len;i++)
		str[i]=(char)toupper(str[i]);
	return str;
}

string itoa(uint x){
	if(!x)return"0";
	string ret;
	while(x){
		ret="0123456789"[x%10]+ret;
		x/=10;
	}
	return ret;
}

uint ReadHex(istream&in,uint digits){
	uint ret;
	char ch;
	eat_white(in).get(ch);
	if((ret=ctoi(ch))==0&&ch!='0'){
		in.unget().clear(ios::badbit);
		return ret;
	}
	for(;--digits;){
		in.get(ch);
		if(ctoi(ch)==0&&ch!='0'){
			in.unget();
			return ret;
		}
		ret<<=4;
		ret|=ctoi(ch);
	}
	return ret;
}

const char*const VALID_PSEUDO="0123456789ABCDEFabcdef \t\v\r\n",
	*const COMMENT="/#;";

#define NPOS (string::npos)

#define checkspriteno()\
	if(spriteno!=-1&&spriteno!=(int)sprites.size() && !_quiet){\
		fprintf(stderr, "Warning: Found sprite %d looking for sprite %d.\n",spriteno,(int)sprites.size());\
	}else(void(0))


#define flush_buffer()\
	if(true){\
		if(buffer!=""){\
			checkspriteno();\
			sprites.push_back(Pseudo(sprites.size(),infover,buffer,claimed_size));\
			buffer="";\
		}\
		spriteno=temp;\
	}else\
		(void(0))

bool is_comment(istream&in){
	if(strchr("#;",in.peek()))return true;
	if(in.peek()!='/')return false;
	in.ignore();
	if(in.peek()=='/')return true;
	in.putback('/');
	return false;
}

bool is_comment(const string&str,int off){
	if(strchr("#;",str[off]))return true;
	if(str[off]!='/'||str[off+1]!='/') return false;
	return true;
}

void read_file(istream&in,int infover,AllocArray<Sprite>&sprites){
	string sprite,datapart,buffer;

	int temp=-1,spriteno=-1,claimed_size=1;
	string::size_type firstnotpseudo;
	while(true){
		getline(in,sprite);
		istringstream spritestream(sprite);
		eat_white(spritestream);
		if(spritestream.peek()==EOF || // blank
			is_comment(spritestream)){ // comment
		}else{//sprite
			if(!eat_white(spritestream>>temp)){
				spritestream.clear();
				temp=-1;
			}
			if(spritestream.peek()=='*'){
				if(spritestream.ignore().peek()=='*'){
					flush_buffer();
					getline(eat_white(spritestream.ignore()),datapart);
					checkspriteno();
					sprites.push_back(Include(datapart));
				}else{
					flush_buffer();
					eat_white(spritestream>>claimed_size);
					getline(spritestream,buffer);
					buffer+='\n';
				}
			}else{
				getline(spritestream,datapart);
				firstnotpseudo=datapart.find_first_not_of(VALID_PSEUDO);
				if((!spritestream||firstnotpseudo==NPOS||
					(datapart[firstnotpseudo]=='"'&&infover>4)||
					(datapart[firstnotpseudo]=='\\'&&infover>6)||
					is_comment(datapart,firstnotpseudo))&&
					Pseudo::MayBeSprite(buffer)){
						buffer+=sprite+'\n';
				}else{
					flush_buffer();
					checkspriteno();
					sprites.push_back(Real(sprites.size(),infover,datapart));
				}
			}
		}
		if(in.peek()==EOF){
			flush_buffer();
			return;
		}
	}
}

Sprite::unparseable::unparseable(string reason,size_t sprite){
	this->reason="Error: "+reason+".\n\tWhile reading sprite:"+itoa((int)sprite)+'\n';
}

Real::Real(size_t sprite,int infover,const string&data){
	string::size_type loc=NPOS;
	string udata=UCase(data);
	while(true){
		loc=udata.find(".PCX",loc+1);
		if(loc==NPOS)
			throw Sprite::unparseable("Could not find filename",sprite);
		if(isspace(data[loc+4]))break;
	}
	if((name=data.substr(0,loc+4))!=prevname){
		prevy=0;
		prevname=name;
	}
	const char*meta=data.c_str()+loc+5;
	if(infover<3){
		unsigned int intinf[8];
		if(sscanf(data.c_str(), "%d %d %x %x %x %x %x %x %x %x",
			&xpos, &ypos,
			&(intinf[0]), &(intinf[1]), &(intinf[2]), &(intinf[3]),
			&(intinf[4]), &(intinf[5]), &(intinf[6]), &(intinf[7]))!=10)
			throw Sprite::unparseable("Insufficient meta-data",sprite);
		for(int i=0;i<8;i++){
			if(intinf[i]>0xFF)
				throw Sprite::unparseable("\"Byte\" "+itoa(i)+" isn't.",sprite);
			inf[i]=U8(intinf[i]);
		}
	}else{
		int sx,sy,rx,ry,comp;
		if(sscanf(meta,"%d %d %2x %d %d %d %d",&xpos,&ypos,&comp,&sy,&sx,&rx,&ry)!=7){
			throw Sprite::unparseable("Insufficient meta-data",sprite);
		}
		if(sx<1)throw Sprite::unparseable("xsize is too small",sprite);
		if(sx>0xFFFF)throw Sprite::unparseable("xsize is too large",sprite);
		if(sy<1)throw Sprite::unparseable("ysize is too small",sprite);
		if(sy>0xFF)throw Sprite::unparseable("ysize is too large",sprite);
		if(rx<-32768)throw Sprite::unparseable("xrel is too small",sprite);
		if(rx>32767)throw Sprite::unparseable("xrel is too large",sprite);
		if(ry<-32768)throw Sprite::unparseable("yrel is too small",sprite);
		if(ry>32767)throw Sprite::unparseable("yrel is too large",sprite);
		inf[0] = U8(comp);
		inf[1] = U8(sy);
		inf[2] = U8(sx & 0xff);
		inf[3] = U8(sx >> 8);
		inf[4] = U8(rx & 0xff);	
		inf[5] = U8(rx >> 8);
		inf[6] = U8(ry & 0xff);
		inf[7] = U8(ry >> 8);
	}
	if (infover < 4)
		ypos++;	// bug, had an extra line at the top
	if(xpos<0)throw Sprite::unparseable("xpos is too small",sprite);
	if(ypos<0)throw Sprite::unparseable("ypos is too small",sprite);
	forcereopen=(ypos<prevy);
	prevy=ypos;
}

string Real::prevname;
int Real::prevy=0;

#define CHAR(x) (char(((ch>>((x)*6))&0x3F)|0x80))

string GetUtf8Encode(uint ch){
	if(ch<0x80)return string()+char(ch);
	if(ch<0x800)return string()+char(((ch>>6 )&0x1F)|0xC0)+CHAR(0);
	/*if(ch<0x10000)*/return string()+char(((ch>>12)&0x0F)|0xE0)+CHAR(1)+CHAR(0);
	//if(ch<0x200000)return string()+char(((ch>>18)&0x07)|0xF0)+CHAR(2)+CHAR(1)+CHAR(0);
	//if(ch<0x4000000)return string()+char(((ch>>24)&0x03)|0xF8)+CHAR(3)+CHAR(2)+CHAR(1)+CHAR(0);
	//if(ch<0x80000000)return string()+char(((ch>>30)&0x01)|0xFC)+CHAR(4)+CHAR(3)+CHAR(2)+CHAR(1)+CHAR(0);
	//INTERNAL_ERROR(ch,ch);
}

#undef CHAR

int findescape(string);

Pseudo::Pseudo(size_t num,int infover,const string&sprite,int claimed_size){
	istringstream in(sprite);
	ostringstream out;
	char ch;
	while(in){
		eat_white(in);
		switch(in.peek()){
		case EOF:continue;
		case'"':
			in.ignore();
			while(true){
				if(!in.get(ch))
					throw Sprite::unparseable("Unterminated literal string",num);
				if(ch=='"')
					break;
				if(ch=='\\'&&infover>6){
					switch(ch=(char)in.get()){
					case'n':
						ch='\r';//TTD uses Mac-style linefeeds (\r)
						break;
					case'"':
					case'\\':
						break;
					case'U':{
						uint x=ReadHex(in,4);
						if(!in)
							throw Sprite::unparseable("Could not parse quoted escape sequence",num);
						if(x>0x7FF)
							out.write(GetUtf8Encode(x).c_str(),3);
						else if(x>0x7F)
							out.write(GetUtf8Encode(x).c_str(),2);
						else out.put(x);
						continue;
						}
					default:
						in.unget();
						ch=(char)ReadHex(in,2);
						if(!in)
							throw Sprite::unparseable("Could not parse quoted escape sequence",num);
						break;
					}
				}
				out.put(ch);
			}
			break;
		case'/':case'#':case';'://comment
			in.ignore(INT_MAX,'\n');
			break;
		case'\\':
			if(infover>6){
				in.ignore();
				uint x;
				switch(in.get()){
				case'b':
					if(in.peek()=='*'){// \b*
						x = ReadValue(in.ignore(), _BX_, num);
						if(!in||x>0xFFFF)break;//invalid
						if(x>0xFE){
							out.put('\xFF');
							out.put(x);
							out.put(x>>8);
						}else out.put((char)x);
						continue;
					}
					x = ReadValue(in, _B_, num);
					if(!in||x>0xFF)break;//invalid
					out.put((char)x);
					continue;
				case'w':
					x = ReadValue(in, _W_, num);
					if(!in||x>0xFFFF)break;//invalid
					out.put(x);
					out.put(x>>8);
					continue;
				case'd':
					x = ReadValue(in, _D_, num);
					if(!in)break;
					out.put(x);
					out.put(x>>8);
					out.put(x>>16);
					out.put(x>>24);
					continue;
				default:{
					in.unget();
					string esc;
					in>>esc;
					int byte = findescape(esc);
					if(byte == -1) break;
					out.put(byte);
					continue;
				}}
				throw Sprite::unparseable("Could not parse unquoted escape sequence",num);
			}
		default:
			ch=(char)ReadHex(in,2);
			if(!in)
				throw Sprite::unparseable("Encountered invalid character looking for literal byte",num);
			out.put(ch);
		}
	}
	packed=out.str();
	if(!size())
		throw Sprite::unparseable("Found a zero-byte pseudo-sprite",num);
	if(size()!=(uint)claimed_size&&claimed_size!=0 && !_quiet)
		fprintf(stderr, "Warning: Sprite %d reports %d bytes, but I found %d.\n",(int)num,claimed_size,size());
}

uint Pseudo::size()const{return (uint)packed.size();}

bool Pseudo::MayBeSprite(const string&sprite){
	istringstream in(sprite);
	char ch;
	while(in.get(ch)){
		if(ch=='"')return true;
		if(strchr(COMMENT,ch)){
			in.ignore(INT_MAX,'\n');
			continue;
		}
		if(isspace(ch)||string(VALID_PSEUDO).find(ch)==NPOS)continue;
		return true;
	}
	return false;
}

Include::Include(const string&data):name(data){}

#ifdef NO_BOOST
uint Pseudo::ReadValue(istream& in, width w, size_t num)
#else
uint Pseudo::_ReadValue(istream& in, width w)
#endif
{
	if (in.peek() == 'x') {		// Read any hex value
		uint ret;
		in.ignore()>>setbase(16)>>ret>>setbase(10);
		return ret;
	}
	/*if (in.peek() == '(') {		// Read any RPN value
		//TODO: Magic goes here
	}*/

	// Read any other value
	string str;
	in>>str;
	char c1, c2;
	int y, m, d, count = sscanf(str.c_str(), "%d%c%d%c%d", &y, &c1, &m, &c2, &d);

	if (count==1) {
		// Got a decimal number
		if (w==_B_ && y>1920) y-=1920;	// special case for byte-sized years
		return y;
	}

	// May have a date. Check, fiddle, and invoke date_time.
	if (count == 5 && c1 == c2 && (c1 == '-' || c1 == '/')) {
#ifdef NO_BOOST
		throw unparseable("This GRFCodec was compiled without support for dates",num);
#else
		int extra = 0;

		if (w == _W_) {
			// word date
			if (d==0 || (d>31 && d<100) || d>1919) swap(y, d);	// Try DMY instead
			if (y==0) y = 2000;
			else if (y>31 && y<100) y+=1900;
		} else if (w == _D_) {
			// dword date
			extra = 701265;
			if (d >= 32) swap(y, d); // Try DMY instead
			// Boost doesn't support years out of the range 1400..9999
			while (y>9999) {
				y -= 400;
				extra += 365*400 + 97; // 97 leap years every 400 years.
			}
			while (y<1400) {
				y += 400;
				extra -= 365*400 + 97;
			}
		} else goto fail;		// I can't read a date of that width.

		try {
			return (date((ushort)y, (ushort)m, (ushort)d) - date(1920, 1, 1)).days() + extra;
		} catch (std::out_of_range) {
			// Fall through to fail
		}
#endif
	}

#ifndef NO_BOOST
fail:		// unreferenced ifdef NO_BOOST
#endif
	// Nothing worked
	in.clear(ios::badbit);
	return (uint)-1;
}