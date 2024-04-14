//時間を計測する並列処理BPA
//更にこれはOBJファイルを書き出すプログラム
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "mpi.h"
#include <omp.h>

// 座標を表す構造体
typedef struct {
    double x;
    double y;
} Point;

// 三角形の辺の長さを表す構造体
typedef struct {
    double a_edge;
    double b_edge;
    double c_edge;
} Edge;

typedef struct Mesh {
    int p1;
    int p2;
    int p3;
} Mesh;

// 二点間の距離を計算する関数
double distance(Point a, Point b) {
    return sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

// 外接円の半径を計算する関数
double calculate_radius(Point a, Point b, Point c) {
    // 三辺の長さを計算
    double a_edge = distance(a, b);
    double b_edge = distance(b, c);
    double c_edge = distance(c, a);

    // ヘロンの公式による面積計算
    double s = (a_edge + b_edge + c_edge) / 2;
    double area = sqrt(s * (s - a_edge) * (s - b_edge) * (s - c_edge));

    // 外接円の半径計算
    double R = a_edge * b_edge * c_edge / (4 * area);
    return R;
}

// objファイルから頂点座標情報だけを取り出す関数
void extract_vertex_from_obj_file(const char* file_path, Point** point_list, int* point_count) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        printf("Unable to open file.\n");
        return;
    }

    char line[256];
    Point temp_points[10000];
    *point_count = 0;

    while (fgets(line, sizeof(line), file)) {//fileから一行ずつ文字列を読み取り，lineに格納
        if (line[0] == 'v' && line[1] == ' ') {//文字列の先頭がvのみの文字列であるか
            Point point;
            double y_point;
            sscanf(line, "v %lf %lf %lf", &point.x, &y_point, &point.y);//文字列からxとz座標のみ抽出する．%*lfと書くと抽出が飛ばされる
            temp_points[(*point_count)++] = point;//配列に格納
        }
    }

    fclose(file);

    *point_list = (Point*)malloc(sizeof(Point) * (*point_count));//メモリ割り当て
    memcpy(*point_list, temp_points, sizeof(Point) * (*point_count));//point_listへtemp_pointsをコピー
}

//テキストファイルにdouble型の配列を書き込む
void writeNumbersToFile(const double* numbers, int numCount, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (file == NULL) {
        printf("ファイルを開くことができませんでした。\n");
        return;
    }

    for (int i = 0; i < numCount; i++) {
        fprintf(file, "%f\n", numbers[i]);
    }

    fclose(file);
}

int main( argc, argv )
int argc;
char **argv;
{
    double start_time, end_time, elapsed_time;
    double first_start_time, first_end_time, first_elapsed_time;
    int myrank;/* number of each node */
    int size;/* The number of process */
    MPI_Status status;
    int rnode;/* right node */
    int lnode;/* left node */
    int i;
    /* MPI initiation and important setting */
    MPI_Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &myrank );
    MPI_Comm_size( MPI_COMM_WORLD, &size );
    if(myrank==0){
            printf("No. of process = %d.\n",size);
            printf("Time tick = %16.6E seconds.\n",MPI_Wtick());
    }
    rnode=(myrank+1)%size;
    lnode=(myrank-1+size)%size;
    //printf("I am %d, right node=%d, left node=%d.\n",myrank, rnode,lnode);

    start_time = MPI_Wtime(); // 全体処理の開始時間を記録

    double *firstTime_list = NULL;
    int loop_count = 0;//ループ回数を数える

    int mesh_count = 0;//メッシュ数を数える

    Point* point_list = NULL;
    int point_count = 0;//点数を数える
    char *fpath = "/home/kit/GitCode_mywork/High-performance_parallel_processing/myProgram/3ddata/usagi.obj";//objファイルのパス
    //char *fpath = "/home/kit/GitCode_mywork/High-performance_parallel_processing/myProgram/3ddata/usagi0_006.obj";//objファイルのパス
    //char *fpath = "/home/kit/GitCode_mywork/High-performance_parallel_processing/myProgram/3ddata/usagi0_004.obj";//objファイルのパス
    //char *fpath = "/home/kit/GitCode_mywork/High-performance_parallel_processing/myProgram/3ddata/usagi0_003.obj";//objファイルのパス
    //char *fpath = "/home/kit/GitCode_mywork/High-performance_parallel_processing/myProgram/3ddata/usagi0_002.obj";//objファイルのパス

    extract_vertex_from_obj_file(fpath, &point_list, &point_count);//objファイルから点座標取得

    MPI_Barrier(MPI_COMM_WORLD);//全ランクの同期を取る

    // Adjust the loop to run over a portion of the data based on the MPI rank
    int points_per_rank = point_count / size; // 一つのランクがさばく点数
    int i_start = myrank * points_per_rank;   // 各ランクの処理開始点
    int i_end;                                // 各ランクの処理終了点
    if (myrank != size - 1) {
        i_end = i_start + points_per_rank;
    } else {//最大ランク番号のランクだけ実行される
        // The last rank picks up the remainder
        i_end = point_count;
    }
    printf("I am %d, startPoint=%d, endPoint=%d.\n",myrank, i_start, i_end);

    //ここで各ノードが各ノードに割り当てられた点の組み合わせを生成する．同じ配列に格納しているように見えるが，ノード毎に配列は確保されているので，メモリ上では同じ配列ではない．
    //だから安心して各ノードで他のノードと同じインデックスに要素を格納すればよい
    double max_r = 0.01;//外接円最大半径
    for (int i = i_start; i < i_end; ++i) {
        first_start_time = MPI_Wtime(); // 第一for文処理の開始時間を記録
        for (int j = i + 1; j < point_count; ++j) {
            for (int k = j + 1; k < point_count; ++k) {
                double r = calculate_radius(point_list[i], point_list[j], point_list[k]);//外接円計算
                if (r <= max_r) {
                    // メッシュ作成
                    mesh_count++;
                }
            }
        }
        first_end_time = MPI_Wtime(); // 第一for文処理の終了時間を記録
        first_elapsed_time = first_end_time - first_start_time; // 第一for文経過時間を計算
        firstTime_list = (double*)realloc(firstTime_list, (loop_count + 1) * sizeof(first_elapsed_time));
        firstTime_list[loop_count++] = first_elapsed_time;
    }
    //printf("my rank%d have finished making their points\n", myrank);
    MPI_Barrier(MPI_COMM_WORLD);//全ランクの同期を取る

    //時間を書き出す
    char TimeFpath[32];
    sprintf(TimeFpath, "timedata/first_time%d.txt", myrank);
    writeNumbersToFile(firstTime_list, loop_count, TimeFpath);

    if (myrank == 0) {
        printf("点数: %d\n", point_count);
    }
    printf("rank%dのメッシュ数: %d\n", myrank, mesh_count);
    end_time = MPI_Wtime(); // 全体処理の終了時間を記録
    elapsed_time = end_time - start_time; // 全体経過時間を計算
    printf("rank%dの全体時間: %f seconds\n", myrank, elapsed_time);

    free(point_list);//メモリ解放
    free(firstTime_list);//メモリ解放

    MPI_Finalize();
    return 0;
}