#ifndef QUESTION_H
#define QUESTION_H

#include <string>
#include <vector>


class Question{

	private:
		string text;
		vector<string> answers;
		int correct;

	public:
		Question(){}
		Question(string t, string a1, string a2, string a3, string a4, int correct){
			this->text=t;
			this->answers.push_back(a1);
			this->answers.push_back(a2);
			this->answers.push_back(a3);
			this->answers.push_back(a4);
			this->correct = correct;
		}
		string getText(){
			return this->text;
		}
		string getAnswer(int index){
			if((index >=0) && (index<4)){
				return this->answers[index];
			}
			//Bad exception handling, this should never happen anyway, it's only for debug
			return "If you see this then check getAnswer in Question class";
		}
		int getCorrect(){
			return this->correct;
		}
};

#endif