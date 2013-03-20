// Overview of edge.js: http://tjanczuk.github.com/edge

//#r "System.Core.dll"
//#r "System.Data.dll"
//#r "System.Data.DataSetExtensions.dll"
//#r "System.Runtime.Serialization.dll"
//#r "System.ServiceModel.dll"
//#r "System.Xml.dll"
//#r "System.Xml.Linq.dll"

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.ServiceModel;
using System.ServiceModel.Channels;
using ServiceReference1;

public class Startup
{
    public async Task<object> Invoke(double kilograms)
    {
        ConvertWeightsSoapClient client = new ConvertWeightsSoapClient(
            new BasicHttpBinding(),
            new EndpointAddress("http://www.webservicex.net/ConvertWeight.asmx"));

        var result = new
        {
            pounds = await client.ConvertWeightAsync(kilograms, WeightUnit.Kilograms, WeightUnit.PoundsTroy),
            ounces = await client.ConvertWeightAsync(kilograms, WeightUnit.Kilograms, WeightUnit.OuncesTroyApoth)
        };
        
        return result;
    }
}

// The rest of this file was auto-generated using Visual Studio's Add Service Reference feature

namespace ServiceReference1
{


    [System.CodeDom.Compiler.GeneratedCodeAttribute("System.ServiceModel", "4.0.0.0")]
    [System.ServiceModel.ServiceContractAttribute(Namespace = "http://www.webserviceX.NET/", ConfigurationName = "ServiceReference1.ConvertWeightsSoap")]
    public interface ConvertWeightsSoap
    {

        [System.ServiceModel.OperationContractAttribute(Action = "http://www.webserviceX.NET/ConvertWeight", ReplyAction = "*")]
        [System.ServiceModel.XmlSerializerFormatAttribute(SupportFaults = true)]
        double ConvertWeight(double Weight, ServiceReference1.WeightUnit FromUnit, ServiceReference1.WeightUnit ToUnit);

        [System.ServiceModel.OperationContractAttribute(Action = "http://www.webserviceX.NET/ConvertWeight", ReplyAction = "*")]
        System.Threading.Tasks.Task<double> ConvertWeightAsync(double Weight, ServiceReference1.WeightUnit FromUnit, ServiceReference1.WeightUnit ToUnit);
    }

    /// <remarks/>
    [System.CodeDom.Compiler.GeneratedCodeAttribute("System.Xml", "4.0.30319.17929")]
    [System.SerializableAttribute()]
    [System.Xml.Serialization.XmlTypeAttribute(Namespace = "http://www.webserviceX.NET/")]
    public enum WeightUnit
    {

        /// <remarks/>
        Grains,

        /// <remarks/>
        Scruples,

        /// <remarks/>
        Carats,

        /// <remarks/>
        Grams,

        /// <remarks/>
        Pennyweight,

        /// <remarks/>
        DramAvoir,

        /// <remarks/>
        DramApoth,

        /// <remarks/>
        OuncesAvoir,

        /// <remarks/>
        OuncesTroyApoth,

        /// <remarks/>
        Poundals,

        /// <remarks/>
        PoundsTroy,

        /// <remarks/>
        PoundsAvoir,

        /// <remarks/>
        Kilograms,

        /// <remarks/>
        Stones,

        /// <remarks/>
        QuarterUS,

        /// <remarks/>
        Slugs,

        /// <remarks/>
        weight100UScwt,

        /// <remarks/>
        ShortTons,

        /// <remarks/>
        MetricTonsTonne,

        /// <remarks/>
        LongTons,
    }

    [System.CodeDom.Compiler.GeneratedCodeAttribute("System.ServiceModel", "4.0.0.0")]
    public interface ConvertWeightsSoapChannel : ServiceReference1.ConvertWeightsSoap, System.ServiceModel.IClientChannel
    {
    }

    [System.Diagnostics.DebuggerStepThroughAttribute()]
    [System.CodeDom.Compiler.GeneratedCodeAttribute("System.ServiceModel", "4.0.0.0")]
    public partial class ConvertWeightsSoapClient : System.ServiceModel.ClientBase<ServiceReference1.ConvertWeightsSoap>, ServiceReference1.ConvertWeightsSoap
    {

        public ConvertWeightsSoapClient()
        {
        }

        public ConvertWeightsSoapClient(string endpointConfigurationName) :
            base(endpointConfigurationName)
        {
        }

        public ConvertWeightsSoapClient(string endpointConfigurationName, string remoteAddress) :
            base(endpointConfigurationName, remoteAddress)
        {
        }

        public ConvertWeightsSoapClient(string endpointConfigurationName, System.ServiceModel.EndpointAddress remoteAddress) :
            base(endpointConfigurationName, remoteAddress)
        {
        }

        public ConvertWeightsSoapClient(System.ServiceModel.Channels.Binding binding, System.ServiceModel.EndpointAddress remoteAddress) :
            base(binding, remoteAddress)
        {
        }

        public double ConvertWeight(double Weight, ServiceReference1.WeightUnit FromUnit, ServiceReference1.WeightUnit ToUnit)
        {
            return base.Channel.ConvertWeight(Weight, FromUnit, ToUnit);
        }

        public System.Threading.Tasks.Task<double> ConvertWeightAsync(double Weight, ServiceReference1.WeightUnit FromUnit, ServiceReference1.WeightUnit ToUnit)
        {
            return base.Channel.ConvertWeightAsync(Weight, FromUnit, ToUnit);
        }
    }
}
