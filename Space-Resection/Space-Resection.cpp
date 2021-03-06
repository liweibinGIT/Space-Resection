// SpaceResection.cpp: 定义控制台应用程序的入口点。
// 

#include "stdafx.h"
#include "Matrix.h"			// 矩阵运算头文件
#include <iostream>
#include <fstream>
#include <math.h>
#include <Windows.h>

#define N 4					// 控制点数
#define PI 3.1415926
#define MAXITERATIONS 8		// 允许的最大迭代次数
#define PRECISION 3.0e-5	// 精度（角元素的改正数应 < 0.1' = 3.0e-5)

using namespace std;

/// 定义结构体
struct EOEO {				// 外方位元素 Elements 0f Exterior Orientation
	double Xs;				// 外方位元素线元素
	double Ys;
	double Zs;
	double phi;				// 外方位元素角元素	
	double omega;
	double kappa;
};

struct SrcData {			// 已知数据，共四对已知控制点的影像坐标及地面坐标
	double x;				// 影像 x 坐标
	double y;				// 影像 y 坐标
	double X;				// 地面 X 坐标
	double Y;				// 地面 Y 坐标
	double Z;				// 地面 Z 坐标
};

/// 函数声明
void InitInterface();												// 初始化界面
bool CheckPrecison(Matrix &Correction);								// 检查是否收敛
void DataReading(SrcData* sd, char* filename);						// 数据读取
void Revolving(SrcData sd[N], double m, double f);					// 解算
void OutputResult(EOEO* eoeo, Matrix &R, double* M, double m0);		// 结果输出

/// 主函数
int main() {

	SrcData sd[N];
	double m = 15000.0;				// 摄影比例尺 1/m = 1/15000
	double f = 0.15324;				// 主距 f = 153.24mm = 0.15324m
	char filename[50] = "data.txt";	// 储存已知坐标数据的文件名为 data.txt，位于在当前路径下

	InitInterface();
	cout << endl << "> 敲击回车键开始解算" << endl;
	getchar();

	DataReading(sd, filename);		// 读取数据
	Revolving(sd, m, f);			// 解算
	
	return 0;
}

/// --------------------------------------------------------------------------------
///	@ Function ---> 初始化控制台界面
/// --------------------------------------------------------------------------------
void InitInterface() {

	SetConsoleTitle(L"单像空间后方交会程序");
	cout << "【单像空间后方交会程序（C++）】" << endl;
	cout << "遥感信息工程学院 张济帆 2016302590060" << endl;
}

/// --------------------------------------------------------------------------------
///	@ Function ---> 读取坐标数据
///	@ Parameters -> sd：保存已知数据的结构体数组；filename：保存数据的文件名
///	@ Return -----> void
/// --------------------------------------------------------------------------------
void DataReading(SrcData* sd, char* filename) {

	fstream datafile(filename, ios::in | ios::out);
	if (!datafile) {							// 检查文件是否存在
		cout << "Error: 打开文件失败，请检查文件是否存在！" << endl;
		exit(1);								// 非正常退出
	}

	memset(sd, 0, N * sizeof(SrcData));			// 为数组sd分配内存空间

	for (int i = 0; i < N; i++) {
		datafile >> sd[i].x >> sd[i].y >> sd[i].X >> sd[i].Y >> sd[i].Z;
	}
	datafile.close();							// 关闭文件

	cout << "\n> 数据读取完毕..." << endl;
	cout << "\t影像坐标(mm)\t\t\t" << "地面坐标(m)\t" << endl;
	for (int i = 0; i < N; i++) {
		cout << i << "\t" << sd[i].x << "\t" << sd[i].y << "\t\t" << sd[i].X << "\t\t" << sd[i].Y << "\t\t" << sd[i].Z << endl;
	}
}

/// --------------------------------------------------------------------------------
///	@ Function ---> 检查改正数是否已经达到了精度要求
///	@ Parameters -> corrections：保存改正数的数组
///	@ Return -----> 布尔值
/// --------------------------------------------------------------------------------
bool CheckPrecison(Matrix &X) {

	bool Boolean;
	Boolean = { fabs(X.data[3][0]) < PRECISION && fabs(X.data[4][0]) < PRECISION && fabs(X.data[5][0]) < PRECISION };

	return Boolean;
}

/// --------------------------------------------------------------------------------
///	@ Function ---> 迭代器，计算的主体部分
///	@ Parameters -> sd：保存原始数据的结构体数组；m：摄影比例尺的分母；f：摄影机主距
///	@ Return -----> void
/// --------------------------------------------------------------------------------
void Revolving(SrcData sd[N], double m, double f) {

	double phi, omega, kappa, Xs, Ys, Zs;	// 外方位元素
	Xs = Ys = Zs = 0.0;

	// 确定未知数的初始值
	for (int i = 0; i < N; i++) {
		sd[i].x /= 1000;		// 单位换算 mm -> m
		sd[i].y /= 1000;
		Xs += sd[i].X;
		Ys += sd[i].Y;
	}

	Xs /= N;
	Ys /= N;
	Zs = m * f;
	phi = omega = kappa = 0.0;	// κ 的初始值也取 0

	cout << endl << "比例尺为 1:" << m << "，摄影主距为 "<< f << "mm" << endl;
	cout << "\n> 外方位元素的初始值为：" << endl;
	cout << "Xs: " << Xs << "\t" << "Ys: " << Ys << "\t" << "Zs: " << Zs << "\t" << endl;
	cout << "φ: " << phi << "\t\t" << "ω: " << omega << "\t\t" << "κ: " << kappa << "\t\t" << endl;

	double x0(0), y0(0);			// 内方位元素，假设为 0
	double X0[N] = { 0.0 };
	double Y0[N] = { 0.0 };
	double Z0[N] = { 0.0 };

	Matrix R(3, 3);
	Matrix Correction(6, 1);		// 改正数矩阵，即 X = [ΔXs ΔYs ΔZs Δφ Δω Δκ]^T

	// 当只有一个控制点时：V_i = A_i × X - L_i
	Matrix V_i(2, 1);				// V_i = [vx vy]^T	
	Matrix A_i(2, 6);				// A_i 为误差方程系数矩阵	
	Matrix L_i(2, 1);				// L_i = [lx ly]^T = [x - (x), y - (y)]^T
	// （x，y 为观测值；vx，ny 为观测值的改正数；lx，ly 为误差方程的常数项）

	// 当有 N 个控制点时：V = A × X - L
	Matrix V(8, 1);					// V = [V_1 V_2 ... V_N]^T	
	Matrix A(8, 6);					// A = [A_1 A_2 ... A_N]^T
	Matrix L(8, 1);					// L = [L_1 L_2 ... L_N]^T

	Matrix ATA(6, 6);
	Matrix ATL(6, 1);

	int iCount = 0;					// 迭代次数
	cout << "\n> 开始迭代运算..." << endl;
	while (iCount == 0 || !CheckPrecison(Correction)) {								// 未收敛时，进行迭代
		cout << endl << "*************************************************" << endl;
		cout << endl << ">> 第 " << ++iCount << " 次迭代：" << endl;
		if (iCount == MAXITERATIONS) {
			cout << "迭代次数超限，可能不收敛" << endl;
			break; 
		}

		// 计算旋转矩阵（9 个方向余弦）
		R.data[0][0] = cos(phi) * cos(kappa) - sin(phi) * sin(omega) * sin(kappa);	// a1
		R.data[0][1] = -cos(phi) * sin(kappa) - sin(phi) * sin(omega) * cos(kappa);	// a2
		R.data[0][2] = -sin(phi) * cos(omega);										// a3
		R.data[1][0] = cos(omega) * sin(kappa);										// b1
		R.data[1][1] = cos(omega) * cos(kappa);										// b2
		R.data[1][2] = -sin(omega);													// b3
		R.data[2][0] = sin(phi) * cos(kappa) + cos(phi) * sin(omega) * sin(kappa);	// c1
		R.data[2][1] = -sin(phi) * sin(kappa) + cos(phi) * sin(omega) * cos(kappa);	// c2
		R.data[2][2] = cos(phi) * cos(omega);										// c3

		

		for (int i = 0; i < N; i++) {
			Z0[i] = R.data[0][2] * (sd[i].X - Xs) + R.data[1][2] * (sd[i].Y - Ys) + R.data[2][2] * (sd[i].Z - Zs);						// Z0 = Z^bar = a3(X - Xs) + b3(Y - Ys) + c3(Z - Zs)
			X0[i] = x0 - f * (R.data[0][0] * (sd[i].X - Xs) + R.data[1][0] * (sd[i].Y - Ys) + R.data[2][0] * (sd[i].Z - Zs)) / Z0[i];	// X0 = x0 - f * X^bar / Z^bar，为像点坐标的近似值 (x)i
			Y0[i] = y0 - f * (R.data[0][1] * (sd[i].X - Xs) + R.data[1][1] * (sd[i].Y - Ys) + R.data[2][1] * (sd[i].Z - Zs)) / Z0[i];	// Y0 = y0 - f * Y^bar / Z^bar，为像点坐标的近似值 (y)i

			// 计算误差方程式的系数
			A_i.data[0][0] = (R.data[0][0] * f + R.data[0][2] * (sd[i].x - x0)) / Z0[i];											// a11
			A_i.data[0][1] = (R.data[1][0] * f + R.data[1][2] * (sd[i].x - x0)) / Z0[i];											// a12
			A_i.data[0][2] = (R.data[2][0] * f + R.data[2][2] * (sd[i].x - x0)) / Z0[i];											// a13
			A_i.data[0][3] = (sd[i].y - y0) * sin(omega) - ((sd[i].x - x0) * ((sd[i].x - x0) * cos(kappa) - 
						   (sd[i].y - y0) * sin(kappa)) / f + f * cos(kappa)) * cos(omega);											// a14
			A_i.data[0][4] = -f * sin(kappa) - (sd[i].x - x0) * ((sd[i].x - x0) * sin(kappa) + (sd[i].y - y0) * cos(kappa)) / f;	// a15
			A_i.data[0][5] = sd[i].y - y0;																							// a16

			A_i.data[1][0] = (R.data[0][1] * f + R.data[0][2] * (sd[i].y - y0)) / Z0[i];											// a21
			A_i.data[1][1] = (R.data[1][1] * f + R.data[1][2] * (sd[i].y - y0)) / Z0[i];											// a22
			A_i.data[1][2] = (R.data[2][1] * f + R.data[2][2] * (sd[i].y - y0)) / Z0[i];											// a23
			A_i.data[1][3] = -(sd[i].x - x0) * sin(omega) - ((sd[i].y - y0) * ((sd[i].x - x0) * cos(kappa) -
							(sd[i].y - y0) * sin(kappa)) / f - f * sin(kappa)) * cos(omega);										// a24
			A_i.data[1][4] = -f * cos(kappa) - (sd[i].y - y0) * ((sd[i].x - x0) * sin(kappa) + (sd[i].y - y0) * cos(kappa)) / f;	// a25
			A_i.data[1][5] = -(sd[i].x - x0);																						// a26

			L_i.data[0][0] = sd[i].x - X0[i];		// lx = x - (x)
			L_i.data[1][0] = sd[i].y - Y0[i];		// ly = y - (y)

			for (int j = 0; j < 2; j++) {
				L.data[2 * i + j][0] = L_i.data[j][0];			// 累加 L_i 入 L
				for (int k = 0; k < 6; k++) {
					A.data[2 * i + j][k] = A_i.data[j][k];		// 累加 A_i 入 A
				}
			}

			//cout << "A_" << i << " = ";
			//A_i.print();
		}// for

		ATA = A.t() * A;
		ATL = A.t() * L;

		//cout << "A = " << endl;
		//A.print();
		//cout << "L = " << endl;
		//L.print();
		//cout << "ATA = " << endl;
		//ATA.print();
		//cout << "ATL = " << endl;
		//ATL.print();
		//Matrix Inv = ATA.inv();
		//cout << "Inv = ";
		//Inv.print();
		
		Correction = ATA.inv() * ATL;							// 法方程解的表达式

		// correction 即为改正数，每一次迭代时用未知数近似值与上次迭代计算的改正数之和作为新的近似值
		Xs		= Xs	 + Correction.data[0][0];
		Ys		= Ys	 + Correction.data[1][0];
		Zs		= Zs	 + Correction.data[2][0];
		phi		= phi	 + Correction.data[3][0];
		omega	= omega	 + Correction.data[4][0];
		kappa	= kappa  + Correction.data[5][0];


		cout << "【改正数值】：" << endl;
		cout << "\tΔXs = " << Correction.data[0][0] << "\tΔYs = " << Correction.data[1][0] << "\tΔZs = " << Correction.data[2][0] << endl;
		cout << "\tΔφ = " << Correction.data[3][0] << "\tΔω = " << Correction.data[4][0] << "\tΔκ = " << Correction.data[5][0] << endl;

		cout << "\n【外方位元素值】：" << endl;
		cout << "\tXs = " << Xs << "\tYs = " << Ys << "\tZs = " << Zs << endl;
		cout << "\tφ = " << phi << "\tω = " << omega << "\tκ = " << kappa << endl;
	}// while

	EOEO eoeo;		// 外方位元素
	eoeo.Xs		= Xs;
	eoeo.Ys		= Ys;
	eoeo.Zs		= Zs;
	eoeo.phi	= phi;
	eoeo.omega	= omega;
	eoeo.kappa	= kappa;

	cout << endl << ">>> 正常退出迭代，迭代次数为 "<< iCount << " <<<" << endl << endl;

	// 精度评定

	vector<vector<double>> Q(6, vector<double>(1));
	for (int i = 0; i < 6; i++) {
		Q[i][0] = ATA.data[i][i];
	}

	double m0(0);	// 单位权中误差
	double vv(0);	// [vv]，即平方和

	V = A * Correction - L;			// 当有 N 个控制点时：V = A × X - L

	for (int i = 0; i < 8; i++) {
		vv = vv + V.data[i][0] * V.data[i][0];
	}
	m0 = sqrt(vv / (2 * N - 6));			// 中误差 m0

	double M[6] = { 0.0 };					// 保存六个值的中误差

	for (int i = 0; i < 6; i++) {
		double Qi = Q[i][0];
		M[i] = m0 * sqrt(Qi);
		if (i > 2) {
			M[i] = M[i] * 180 * 3600 / PI;	// 转换为角度制
		}
	}

	OutputResult(&eoeo, R, M, m0);		// 输出结果
	cout << endl << ">>> 解算全部完成 <<<" << endl << endl;
}

/// --------------------------------------------------------------------------------
///	@ Function ---> 输出解算结果
///	@ Parameters -> eoeo：指向最终解算结果的结构体数组；R：旋转矩阵；
///					M：保存计算精度的数组；m0：单位权中误差
///	@ Return -----> void
/// --------------------------------------------------------------------------------
void OutputResult(EOEO* eoeo, Matrix &R, double* M, double m0) {

	cout << endl << "/////------------------------------/////" << endl;
	cout << "/////           计算结果           /////" << endl;
	cout << "/////------------------------------/////" << endl << endl;
	cout << "六个外方位元素为：" << endl;
	printf("Xs = %.4f\n", eoeo->Xs);
	printf("Ys = %.4f\n", eoeo->Ys);
	printf("Zs = %.4f\n", eoeo->Zs);
	printf("φ = %.10f\n", eoeo->phi);
	printf("ω = %.10f\n", eoeo->omega);
	printf("κ = %.10f\n", eoeo->kappa);

	cout << endl << "旋转矩阵：" << endl;
	R.print();

	cout << "\n单位权中误差：" << m0 << endl << endl;

	cout << "六个外方位元素的精度：" << endl;
	cout << "Xs:\t" << M[0] << " m" << endl;
	cout << "Ys:\t" << M[1] << " m" << endl;
	cout << "Zs:\t" << M[2] << " m" << endl;
	cout << "φ:\t" << M[3] << "'" << endl;
	cout << "ω:\t" << M[4] << "'" << endl;
	cout << "κ:\t" << M[5] << "'" << endl;
}
