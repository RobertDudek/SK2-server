#ifndef PLAYER_H
#define PLAYER_H

#include <string>



class Player{
	
	private:
		string nick;
		int socketDescriptor;
		int result = 0;

	public:
		Player(string n, int SD){
			this->nick=n;
			this->socketDescriptor = SD;
		}
		string getNick(){
			return this->nick;
		}
		int getSocketDescriptor(){
			return this->socketDescriptor;
		}
		int getResult(){
			return this->result;
		}
		void incResult(){
			this->result++;
		}

};

#endif