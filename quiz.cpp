#ifndef QUIZ_H
#define QUIZ_H 

#include "question.cpp"

class Quiz{

	private:
		string name;
		vector<Question> questions;

	public:
		Quiz(){}
		Quiz(string n){
			this->name=n;
		}
		string getName(){
			return this->name;
		}
		void addQuestion(string q, string a1, string a2, string a3, string a4, int correct){
			this->questions.push_back(Question(q,a1,a2,a3,a4,correct));
		}
		int getNumberOfQuestions(){
			return this->questions.size();
		}
		Question getQuestion(unsigned int index){
			return this->questions[index];
		}
		vector<string> getQuestionAsStringVector(int index){
			vector<string> ans;
			ans.push_back(this->questions[index].getText());
			for(int i=0;i<4;i++){
				ans.push_back(this->questions[index].getAnswer(i));
			}
			return ans;
		}
		int getCorrect(int index){
			return this->questions[index].getCorrect();
		}
};

#endif