#include<iostream>
#include<string>
#include<cstdio>
#include<vector>

using namespace std;





int main(){
vector<int> v;


for(int i=0;i<5;i++)
	v.push_back(i);

for(int i=0;i<v.size();i++)
	cout<<"v["<<i<<"]"<<v[i]<<endl;

cout<<endl;

for(int i=0;i<v.size(); i++)
	if(i%2==0){
		cout<<"gonna delete: "<<*(v.begin()+i)<<endl;
		v.erase(v.begin()+i);
	}

cout<<endl;

for(int i=0;i<v.size();i++)
	cout<<"v["<<i<<"]"<<v[i]<<endl;

return 0;
}
