// Overview of edge.js: http://tjanczuk.github.com/edge

//#r "System.dll"
//#r "System.Data.dll"
//#r "System.Web.Extensions.dll"

using System;
using System.Collections.Generic;
using System.Data;
using System.Data.SqlClient;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

public class Startup 
{
    public async Task<object> Invoke(string command)
    {
        string connectionString = Environment.GetEnvironmentVariable("OWIN_SQL_CONNECTION_STRING");

        if (command.StartsWith("select ", StringComparison.InvariantCultureIgnoreCase))
        {
            return await this.ExecuteQuery(connectionString, command);
        }
        else if (command.StartsWith("insert ", StringComparison.InvariantCultureIgnoreCase)
            || command.StartsWith("update ", StringComparison.InvariantCultureIgnoreCase)
            || command.StartsWith("delete ", StringComparison.InvariantCultureIgnoreCase))
        {
            return await this.ExecuteNonQuery(connectionString, command);
        }
        else 
        {
            throw new InvalidOperationException("Unsupported type of SQL command. Only select, insert, update, and delete are supported.");
        }
    }

    async Task<object> ExecuteQuery(string connectionString, string commandString)
    {
        List<object> rows = new List<object>();

        using (SqlConnection connection = new SqlConnection(connectionString))
        {
            using (SqlCommand command = new SqlCommand(commandString, connection))
            {
                await connection.OpenAsync();
                using (SqlDataReader reader = await command.ExecuteReaderAsync(CommandBehavior.CloseConnection))
                {
                    object[] fieldNames = new object[reader.FieldCount];
                    for (int i = 0; i < reader.FieldCount; i++)
                    {
                        fieldNames[i] = reader.GetName(i);
                    }
                    rows.Add(fieldNames);

                    IDataRecord record = (IDataRecord)reader;                        
                    while (await reader.ReadAsync())
                    {
                        object[] resultRecord = new object[record.FieldCount];
                        record.GetValues(resultRecord);
                        for (int i = 0; i < record.FieldCount; i++)
                        {
                            Type type = record.GetFieldType(i);
                            if (type == typeof(byte[]) || type == typeof(char[]))
                            {
                                resultRecord[i] = Convert.ToBase64String((byte[])resultRecord[i]);
                            }
                            else if (type == typeof(Guid) || type == typeof(DateTime))
                            {
                                resultRecord[i] = resultRecord[i].ToString();
                            }
                            else if (type == typeof(IDataReader))
                            {
                                resultRecord[i] = "<IDataReader>";
                            }
                        }

                        rows.Add(resultRecord);
                    }
                }
            }
        }

        return rows;
    }

    async Task<object> ExecuteNonQuery(string connectionString, string commandString)
    {
        using (SqlConnection connection = new SqlConnection(connectionString))
        {
            using (SqlCommand command = new SqlCommand(commandString, connection))
            {
                await connection.OpenAsync();
                return await command.ExecuteNonQueryAsync();
            }
        }
    }
}
