#ifndef GAME_H
#define GAME_H 

#include "quiz.cpp"
#include "player.cpp"

class Game{

	private:
		int id;
		bool started = false;
		Quiz quiz;
		int currentQustion = 0;
		int numberOfAnswers = 0;
		vector<Player> players;
	public:
		Game(){}
		Game(Quiz q, int i){
			this->quiz = q;
			this->id = i;
		}
		vector<Player> getPlayers(){
			return this->players;
		}
		void addPlayer(string nick, int socketDescriptor){
			this->players.push_back(Player(nick, socketDescriptor));
		}
		void removePlayer(string nick){
			for(unsigned int i=0;i<this->players.size();i++){
				if(this->players[i].getNick() == nick){
					this->players.erase(this->players.begin()+i);
					break;
				}
			}
		}
		int getId(){
			return this->id;
		}
		string getNick(unsigned int index){
			return this->players[index].getNick();
		}
		int getPlayerSD(int index){
			return this->players[index].getSocketDescriptor();
		}
		int getResult(unsigned int index){
			return this->players[index].getResult();
		}
		vector<string> getNicks(){
			vector<string> nicks;
			for(unsigned int i=0;i<this->players.size();i++){
				nicks.push_back(this->players[i].getNick());
			}
			return nicks;
		}
		bool isEmpty(){
			if(this->players.size() == 0) return true;
			return false;
		}
		int getNumberOfPlayers(){
			return this->players.size();
		}
		bool answer(unsigned int index, unsigned int ans){
			
			this->numberOfAnswers++;

			if((unsigned int)this->quiz.getQuestion(index).getCorrect() == ans){
				this->players[index].incResult();
			}

			if(this->numberOfAnswers == this->getNumberOfPlayers()){
				return true;
			}
			return false;
		}
		bool nextQuestion(){
			//returns false if we exceeded number of questions
			this->currentQustion++;
			this->numberOfAnswers = 0;
			if(this->currentQustion > this->quiz.getNumberOfQuestions()){
				return false;
			}
			return true;
		}
		vector<string> getCurrentQuestion(){
			return this->quiz.getQuestionAsStringVector(this->currentQustion);
		}
};

#endif