#include <vector>
#include <fstream>
#include <iostream>
#include "DataStructures.hpp"
#include "Globals.hpp"
#include "MyFileSystem.hpp"

using namespace std;

namespace HCSearch
{
	/**************** Constants ****************/

	const string SearchTypeStrings[] = {"ll", "hl", "lc", "hc", "learnh", "learnc", "learncoracle"};
	const string DatasetTypeStrings[] = {"test", "train", "validation"};

	/**************** Features and Labelings ****************/

	ImgFeatures::ImgFeatures()
	{
	}

	ImgFeatures::~ImgFeatures()
	{
	}

	int ImgFeatures::getFeatureDim()
	{
		return this->graph.nodesData.cols();
	}

	int ImgFeatures::getNumNodes()
	{
		return this->graph.nodesData.rows();
	}

	double ImgFeatures::getFeature(int node, int featIndex)
	{
		return this->graph.nodesData(node, featIndex);
	}

	string ImgFeatures::getFileName()
	{
		return this->filename;
	}

	ImgLabeling::ImgLabeling()
	{
		this->confidencesAvailable = false;
		this->stochasticCutsAvailable = false;
	}

	ImgLabeling::~ImgLabeling()
	{
	}

	int ImgLabeling::getNumNodes()
	{
		return this->graph.nodesData.size();
	}

	int ImgLabeling::getLabel(int node)
	{
		return this->graph.nodesData(node);
	}

	set<int> ImgLabeling::getNeighborLabels(int node)
	{
		set<int> labels;
		if (hasNeighbors(node))
		{
			for (set<int>::iterator it = this->graph.adjList[node].begin();
				it != this->graph.adjList[node].end(); ++it)
			{
				labels.insert(getLabel(*it));
			}
		}
		return labels;
	}

	set<int> ImgLabeling::getNeighbors(int node)
	{
		if (hasNeighbors(node))
			return this->graph.adjList[node];
		else
			return set<int>();
	}

	bool ImgLabeling::hasNeighbors(int node)
	{
		return this->graph.adjList.count(node) != 0;
	}

	/**************** Rank Features ****************/

	RankFeatures::RankFeatures()
	{
	}

	RankFeatures::RankFeatures(VectorXd features)
	{
		this->data = features;
	}

	RankFeatures::~RankFeatures()
	{
	}

	/**************** SVM-Rank Model ****************/

	SVMRankModel::SVMRankModel()
	{
		this->initialized = false;
		this->learningMode = false;
	}

	SVMRankModel::SVMRankModel(string fileName)
	{
		load(fileName);
	}
	
	double SVMRankModel::rank(RankFeatures features)
	{
		if (!this->initialized)
		{
			cerr << "[ERROR] svm ranker not initialized for ranking" << endl;
			exit(1);
		}

		return vectorDot(getWeights(), features.data);
	}

	RankerType SVMRankModel::rankerType()
	{
		return SVM_RANK;
	}
	
	VectorXd SVMRankModel::getWeights()
	{
		if (!this->initialized)
		{
			cerr << "[ERROR] svm ranker not initialized for getting weights" << endl;
			exit(1);
		}

		return this->weights;
	}

	void SVMRankModel::load(string fileName)
	{
		this->weights = parseModelFile(fileName);
		this->initialized = true;
	}

	void SVMRankModel::startTraining(string featuresFileName)
	{
		this->learningMode = true;
		this->qid = 1;
		this->rankingFile = new ofstream(featuresFileName.c_str());
		this->rankingFileName = featuresFileName;
	}

	void SVMRankModel::addTrainingExamples(RankFeatures& better, vector< RankFeatures >& worseSet)
	{
		// write good example
		(*this->rankingFile) << vector2svmrank(better, 1, this->qid) << endl;

		// write bad examples
		for (vector< RankFeatures >::iterator it = worseSet.begin(); it != worseSet.end(); ++it)
		{
			RankFeatures worse = *it;
			(*this->rankingFile) << vector2svmrank(worse, 2, this->qid) << endl;
		}

		// increment qid
		this->qid++;
	}

	void SVMRankModel::finishTraining(string modelFileName)
	{
		if (qid <= 1)
		{
			cerr << "[Error] no training data available for learning!" << endl;
			return;
		}

		// compute C
		double C = 1.0 * (this->qid-1);

		// close ranking file
		this->rankingFile->close();
		delete this->rankingFile;

		// call SVM-Rank
		stringstream ssLearn;
		ssLearn << Global::settings->cmds->SVMRANK_LEARN_CMD << " -c " << C << " " 
			<< this->rankingFileName << " " << modelFileName;
		MyFileSystem::Executable::executeRetries(ssLearn.str());

		// no longer learning
		this->learningMode = false;

		// Load weights into model and initialize
		load(Global::settings->paths->OUTPUT_HEURISTIC_MODEL_FILE);
	}

	void SVMRankModel::cancelTraining()
	{
		// close ranking file
		this->rankingFile->close();
		delete this->rankingFile;

		// no longer learning
		this->learningMode = false;
	}

	VectorXd SVMRankModel::parseModelFile(string fileName)
	{
		string line;
		VectorXd weights;
		vector<int> indices;
		vector<double> values;

		ifstream fh(fileName.c_str());
		if (fh.is_open())
		{
			int lineNum = 0;
			while (fh.good())
			{
				lineNum++;
				getline(fh, line);
				if (lineNum < 12)
					continue;

				if (lineNum == 12)
				{
					istringstream iss(line);

					string token;
					while (getline(iss, token, ' '))
					{
						if (token.find(':') == std::string::npos)
							continue;

						istringstream isstoken(token);
						string sIndex;
						string sValue;
						getline(isstoken, sIndex, ':');
						getline(isstoken, sValue, ':');

						int index = atoi(sIndex.c_str());
						double value = atof(sValue.c_str());

						indices.push_back(index);
						values.push_back(value);
					}
				}
			}
			fh.close();
		}
		else
		{
			cerr << "[ERROR] cannot open model file for reading weights!!" << endl;
			exit(1);
		}

		weights = VectorXd::Zero(indices.back());
		int valuesSize = values.size();
		if (valuesSize == 0)
		{
			cerr << "[ERROR] assigning empty weights from '" + fileName + "'!" << endl;
			exit(1);
		}

		for (int i = 0; i < valuesSize; i++)
		{
			int ind = indices[i]-1;
			weights(ind) = values[i];
		}

		return weights;
	}

	string SVMRankModel::vector2svmrank(RankFeatures features, int target, int qid)
	{
		stringstream ss("");
		stringstream sparse("");

		VectorXd vector = features.data;

		int nonZeroCounts = 0;
		for (int i = 0; i < vector.size(); i++)
		{
			if (vector(i) != 0)
			{
				ss << i+1 << ":" << vector(i) << " ";
				nonZeroCounts++;
			}
		}
		sparse << target << " qid:" << qid << " " << ss.str();

		return sparse.str();
	}

	/**************** Online Rank Model ****************/

	OnlineRankModel::OnlineRankModel()
	{
		this->initialized = false;
	}

	OnlineRankModel::OnlineRankModel(string fileName)
	{
		load(fileName);
	}

	double OnlineRankModel::rank(RankFeatures features)
	{
		if (!initialized)
		{
			cerr << "[ERROR] online ranker not initialized for ranking" << endl;
			exit(1);
		}

		return vectorDot(getAvgWeights(), features.data);
	}

	RankerType OnlineRankModel::rankerType()
	{
		return ONLINE_RANK;
	}

	VectorXd OnlineRankModel::getLatestWeights()
	{
		if (!initialized)
		{
			cerr << "[ERROR] online ranker not initialized for getting latest weights" << endl;
			exit(1);
		}

		return this->latestWeights;
	}

	VectorXd OnlineRankModel::getAvgWeights()
	{
		if (!initialized)
		{
			cerr << "[ERROR] online ranker not initialized for getting avg weights" << endl;
			exit(1);
		}

		return (1.0/this->numSum)*this->cumSumWeights;
	}

	void OnlineRankModel::performOnlineUpdate(double delta, VectorXd featureDiff)
	{
		if (!initialized)
		{
			cerr << "[ERROR] online ranker not initialized for updating" << endl;
			exit(1);
		}

		double tauNumerator = sqrt(delta) - vectorDot(getLatestWeights(), featureDiff);
		double tauDenominator = pow(featureDiff.norm(), 2);

		if (tauDenominator == 0 && tauNumerator == 0)
		{
			//TODO tau indeterminant form error
		}
		else if (tauDenominator == 0 && tauNumerator != 0)
		{
			//TODO tau division by zero error
		}
		else
		{
			double tau = tauNumerator/tauDenominator;
			VectorXd newWeights = getLatestWeights() + tau * featureDiff;

			// perform update
			this->latestWeights = newWeights;
			this->cumSumWeights += newWeights;
			this->numSum += 1;
		}
	}

	void OnlineRankModel::load(string fileName)
	{
		parseModelFile(fileName, this->latestWeights, this->cumSumWeights, this->numSum);
		this->initialized = true;
	}

	void OnlineRankModel::save(string fileName)
	{
		writeModelFile(fileName, this->latestWeights, this->cumSumWeights, this->numSum);
	}

	void OnlineRankModel::initialize(int dim)
	{
		this->latestWeights = VectorXd::Zero(dim);
		this->cumSumWeights = VectorXd::Zero(dim);
		this->numSum = 0;
		this->initialized = true;
	}

	void OnlineRankModel::parseModelFile(string fileName, VectorXd& latestWeights, VectorXd& cumSumWeights, int& numSum)
	{
		string line;
		vector<int> indices;
		vector<double> values;
		vector<int> indices2;
		vector<double> values2;

		ifstream fh(fileName.c_str());
		if (fh.is_open())
		{
			int lineNum = 0;
			while (fh.good())
			{
				lineNum++;
				getline(fh, line);

				if (lineNum == 1)
				{
					numSum = atoi(line.c_str());
				}
				else if (lineNum == 2)
				{
					istringstream iss(line);

					string token;
					while (getline(iss, token, ' '))
					{
						if (token.find(':') == std::string::npos)
							continue;

						istringstream isstoken(token);
						string sIndex;
						string sValue;
						getline(isstoken, sIndex, ':');
						getline(isstoken, sValue, ':');

						int index = atoi(sIndex.c_str());
						double value = atof(sValue.c_str());

						indices.push_back(index);
						values.push_back(value);
					}
				}
				else if (lineNum == 3)
				{
					istringstream iss(line);

					string token;
					while (getline(iss, token, ' '))
					{
						if (token.find(':') == std::string::npos)
							continue;

						istringstream isstoken(token);
						string sIndex;
						string sValue;
						getline(isstoken, sIndex, ':');
						getline(isstoken, sValue, ':');

						int index = atoi(sIndex.c_str());
						double value = atof(sValue.c_str());

						indices2.push_back(index);
						values2.push_back(value);
					}
				}
			}
			fh.close();
		}
		else
		{
			cerr << "[ERROR] cannot open model file for reading weights!!" << endl;
			exit(1);
		}

		cumSumWeights = VectorXd::Zero(indices.back());
		int valuesSize = values.size();
		if (valuesSize == 0)
		{
			cerr << "[ERROR] assigning empty cumsumweights from '" + fileName + "'!" << endl;
			exit(1);
		}
		for (int i = 0; i < valuesSize; i++)
		{
			int ind = indices[i]-1;
			cumSumWeights(ind) = values[i];
		}

		latestWeights = VectorXd::Zero(indices.back());
		int valuesSize2 = values2.size();
		if (valuesSize2 == 0)
		{
			cerr << "[ERROR] assigning empty latestweights from '" + fileName + "'!" << endl;
			exit(1);
		}
		for (int i = 0; i < valuesSize2; i++)
		{
			int ind = indices2[i]-1;
			latestWeights(ind) = values2[i];
		}
	}

	void OnlineRankModel::writeModelFile(string fileName, const VectorXd& latestWeights, const VectorXd& cumSumWeights, int numSum)
	{
		ofstream fh(fileName.c_str());
		if (fh.is_open())
		{
			// write num to file
			fh << numSum << endl;

			// write cumulative sum weights to file
			const int cumSumWeightsLength = cumSumWeights.size();
			for (int i = 0; i < cumSumWeightsLength; i++)
			{
				double val = cumSumWeights(i);
				if (val != 0)
				{
					int ind = i+1;
					fh << ind << ":" << val << " ";
				}
			}
			fh << endl;

			// write latest weights to file
			const int latestWeightsLength = latestWeights.size();
			for (int i = 0; i < latestWeightsLength; i++)
			{
				double val = latestWeights(i);
				if (val != 0)
				{
					int ind = i+1;
					fh << ind << ":" << val << " ";
				}
			}
			fh << endl;

			fh.close();
		}
		else
		{
			cerr << "[ERROR] cannot open model file for writing weights!!" << endl;
			exit(1);
		}
	}
}