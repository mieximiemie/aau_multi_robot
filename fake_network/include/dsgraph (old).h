#ifndef DSGRAPH_H
#define DSGRAPH_H

#include <iostream>
#include <stdlib.h>

class DsGraph
{
    public:
        DsGraph() {
            num_nodes = 0;
        }
    
        void addEdge(int start, int end, float cost) {
            bool contained = false;
            for(int i=0; i<nodes.size(); i++)
                if(start == nodes[i])
                    contained = true;
            
            if(contained) {
                graph.at(start).insert( std::pair<int, float>(end, cost) );
            } else {
                nodes.push_back(start);
                std::map<int, float> column;
                column.insert( std::pair<int, float>(end, cost) );
                graph.insert( std::pair<int, std::map<int, float> >(start, column)) ;
            }
            
        /*
            int local_start, local_end;
            if(start < end) {
                local_start = start;
                local_end = end;
            }
            
            
            if(start > num_nodes) {
                num_nodes = start + 1; //for instance, if start = 1, at least it means we have two nodes in the graph (0 and 1)
                std::map<int, float> column;
                column.insert( std::pair<int, float>(end, cost) );
                graph.insert( std::pair<int, std::map<int, float> >(start, column)) ;
            } else {
                graph.at(start).insert( std::pair<int, float>(end, cost) );
            }
            if(end > num_nodes) {
                num_nodes = end + 1;
                std::map<int, float> column;
                column.insert( std::pair<int, float>(start, cost) );
                graph.insert( std::pair<int, std::map<int, float> >(end, column)) ;
            }
            */
        
        };
        
        bool hasEdge(int start, int end);
        bool findPath(int start, int end, std::vector<int> &path);
        
        void print() {
        
        ROS_ERROR("%lu", graph.size());
        ROS_ERROR("%lu", nodes.size());
            for(int i=0; i<nodes.size(); i++) {
                ROS_ERROR("%lu", graph[nodes[i]].size());
                ROS_ERROR("%f", graph[nodes[i]][0]);
                }
            /*
                for(int j=0; j < graph.at(nodes[i]).size(); j++)
                    printf("%.2f", graph.at(i).at(j));
        /*
            for(int i=0; i<graph.size(); i++) {
                for(int j=0; j<graph.at(i).size(); j++)
                    printf("%.2f", graph.at(i).at(j));
                printf("\n");       
            }
            */
        
        };
    
    private:
        int num_nodes;
        std::map<int, std::map<int,float> > graph;
        std::vector< std::vector<int> > graph2;
        std::vector< std::vector<int> > spanning_tree2;
        
        std::vector<int> nodes;
};

#endif /* DSGRAPH_H */